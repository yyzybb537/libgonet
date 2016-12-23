/*
* Change:
*   @2016-07-20 yyz 当队列中积压了很多超时的待发送数据时, 由于超时导致的无数据可发,
*                   此时再yield就会导致其他逻辑写入更多的数据, 形成雪崩且无法恢复.
*/
#include "tcp_detail.h"
#include <chrono>
#include <boost/bind.hpp>

namespace network {
namespace tcp_detail {

    io_service& GetTcpIoService()
    {
        static io_service ios;
        return ios;
    }

    void TcpSession::Msg::Done(boost_ec const& ec)
    {
        if (tid) {
            co_timer_cancel(tid);
            tid.reset();
        }

        if (cb) {
            // 安全地回调, 防止recursive-callback.
            SndCb caller;
            caller.swap(cb);
            caller(ec);
        }
    }

    TcpSession::TcpSession(shared_ptr<tcp_socket> s,
            shared_ptr<LifeHolder> holder, OptionsData & opt,
            endpoint::ext_t const& endpoint_ext)
        : socket_(s), holder_(holder), recv_buf_(opt.max_pack_size_),
        max_pack_size_shrink_((std::max)(opt.max_pack_size_shrink_, opt.max_pack_size_)),
        max_pack_size_hard_((std::max)(opt.max_pack_size_hard_, opt.max_pack_size_)),
        msg_chan_((std::size_t)-1)
    {
        boost_ec ignore_ec;
        local_addr_ = endpoint(s->native_socket().local_endpoint(ignore_ec), endpoint_ext);
        remote_addr_ = endpoint(s->native_socket().remote_endpoint(ignore_ec), endpoint_ext);
        sending_ = false;

        DebugPrint(dbg_session_alive, "TcpSession construct %s:%d",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
    }

    TcpSession::~TcpSession()
    {
        DebugPrint(dbg_session_alive, "TcpSession destruct %s:%d",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
    }

    void TcpSession::goStart()
    {
        co::initialize_socket_async_methods(socket_->native_handle());
        co::set_et_mode(socket_->native_handle());
        if (opt_.connect_cb_)
            opt_.connect_cb_(GetSession());

        goReceive();
        goSend();
    }

//    static std::string to_hex(const char* data, size_t len)                     
//    {                                                                              
//        static const char hex[] = "0123456789abcdef";                              
//        std::string str;                                                           
//        for (size_t i = 0; i < len; ++i) {                                         
//            str += hex[(unsigned char)data[i] >> 4];                               
//            str += hex[(unsigned char)data[i] & 0xf];                              
//            str += ' ';                                                            
//        }                                                                          
//        return str;                                                                
//    }                                                                              

    void TcpSession::goReceive()
    {
        auto this_ptr = this->shared_from_this();
        go_dispatch(egod_local_thread) [=]{
            auto holder = this_ptr;
            size_t pos = 0;
            for (;;)
            {
                boost_ec ec;
                std::size_t n = 0;
                if (pos >= recv_buf_.size()) {
                    if (recv_buf_.size() >= max_pack_size_hard_)
                        ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_recv_overflow);
                    else {
                        // extand capacity
                        recv_buf_.resize((std::min<uint32_t>)(recv_buf_.size() * 2, max_pack_size_hard_));
                    }
                }

                if (!ec)
                    n = socket_->read_some(buffer(&recv_buf_[pos], recv_buf_.size() - pos), ec);

                if (!ec) {
                    if(n > 0) {
//                        printf("receive %u bytes: %s\n", (unsigned)n, to_hex(&recv_buf_[pos], n).c_str());
                        if (this->opt_.receive_cb_) {
                            size_t consume = this->opt_.receive_cb_(GetSession(), recv_buf_.data(), n + pos);
                            if (consume == (size_t)-1)
                                ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_data_parse_error);
                            else {
                                assert(consume <= n + pos);
                                pos = n + pos - consume;
                                if (pos > 0)
                                    memcpy(&recv_buf_[0], &recv_buf_[consume], pos);

                                if (recv_buf_.size() >= max_pack_size_shrink_ + max_pack_size_shrink_ / 2 &&
                                        pos <= max_pack_size_shrink_ / 2) {
                                    // shrink capacity
                                    recv_buf_.resize(max_pack_size_shrink_);
                                    recv_buf_.shrink_to_fit();
                                }
                            }
                        } else {
                            pos += n;
                        }
                    }
                }

                if (ec) {
                    SetCloseEc(ec);
                    DebugPrint(dbg_session_alive, "TcpSession receive shutdown %s:%d",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port());

                    ShutdownRecv();
                    return ;
                } 
            }
        };
    }

    void TcpSession::Shutdown(bool immediately)
    {
        SetCloseEc(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
        DebugPrint(dbg_session_alive, "TcpSession initiative shutdown. is immediately:%s, remote addr %s:%d",
                immediately ? "true" : "false",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
        initiative_shutdown_ = true;

        if (immediately)
            socket_->shutdown(socket_base::shutdown_both);
        msg_chan_.TryPush(boost::make_shared<Msg>(Msg::shutdown_msg_t{}));
    }

    void TcpSession::ShutdownSend()
    {
        socket_->shutdown(socket_base::shutdown_send);
        send_shutdown_ = true;
        if (recv_shutdown_)
            OnClose();
    }

    void TcpSession::ShutdownRecv()
    {
        socket_->shutdown(socket_base::shutdown_receive);
        recv_shutdown_ = true;
        if (send_shutdown_)
            OnClose();
        else {
            msg_chan_.TryPush(boost::make_shared<Msg>(Msg::shutdown_msg_t{}));
            socket_->shutdown(socket_base::shutdown_send);
        }
    }

    void TcpSession::goSend()
    {
        auto this_ptr = this->shared_from_this();
        go_dispatch(egod_local_thread) [=]{
            auto holder = this_ptr;
            const int c_multi = std::min<int>(64, boost::asio::detail::max_iov_len);
            std::vector<const_buffer> buffers;
            bool msg_shutdown = false;
            for (;;)
            {
                if (msg_shutdown) {
                    DebugPrint(dbg_session_alive, "TcpSession send shutdown with message. %s:%d.",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port());
                    ShutdownSend();
                    return ;
                }

                std::unique_lock<co::LFLock> send_token(send_mtx_);
                int remain = std::max(0, c_multi - (int)msg_send_list_.size());
                int insert_c = 0;
                while (insert_c < remain)
                {
                    boost::shared_ptr<Msg> msg;
                    if (!msg_chan_.TryPop(msg)) {
                        if (msg_send_list_.empty()) {
                            if (initiative_shutdown_) {
                                DebugPrint(dbg_session_alive, "TcpSession send shutdown with initiative_shutdown flag. %s:%d.",
                                        remote_addr_.address().to_string().c_str(), remote_addr_.port());
                                ShutdownSend();
                                return ;
                            } else {
                                sending_ = false;
                                send_token.unlock();
                                msg_chan_ >> msg;
                                send_token.lock();
                            }
                        } else {
                            break;
                        }
                    }

                    if (msg->shutdown) {    // shutdown notify
                        msg_shutdown = true;
                        DebugPrint(dbg_session_alive, "goSend get shutdown msg.");
                        break;
                    } else if (msg->timeout) {
                        msg->Done(MakeNetworkErrorCode(eNetworkErrorCode::ec_send_timeout));
                    } else {
                        ++ insert_c;
                        msg_send_list_.push_back(msg);
                    }
                }

                // Make buffers
                buffers.clear();
                buffers.resize(std::min<int>(msg_send_list_.size(), c_multi));

                std::size_t write_bytes = 0;
                int buffer_size = 0;
                auto it = msg_send_list_.begin();
                while (it != msg_send_list_.end())
                {
                    auto &msg = *it;
                    if (msg->timeout && !msg->send_half) {
                        msg->Done(MakeNetworkErrorCode(eNetworkErrorCode::ec_send_timeout));
                        it = msg_send_list_.erase(it);
                        continue;
                    }

                    if (buffer_size >= c_multi) break;
                    buffers[buffer_size] = buffer(&msg->buf[msg->pos], msg->buf.size() - msg->pos);
                    write_bytes += msg->buf.size() - msg->pos;
                    DebugPrint(dbg_no_delay, "write buffer (pos=%lu, capacity=%lu)",
                            msg->pos, msg->buf.size());

                    ++it;
                    ++buffer_size;
                }
                buffers.resize(buffer_size);
                if (buffers.empty()) {
                    continue;
                }

                // Send Once
                boost_ec ec;
                std::size_t n = 0;
                pollfd pfd = { socket_->native_handle(), POLLOUT, 0 };
                int timeo = opt_.sndtimeo_ > 0 ? std::max(opt_.sndtimeo_ / 2, 1) : -1;

                ::boost::asio::detail::buffer_sequence_adapter<
                    ::boost::asio::const_buffer,
                    std::vector<const_buffer>> bufs(buffers);
retry_write:
                ssize_t nbytes = ::writev_f(socket_->native_handle(), bufs.buffers(), bufs.count());
                if (nbytes < 0) {
                    if (errno == EINTR) {
                        goto retry_write;
                    } else if (errno == EAGAIN) {
retry_poll:
                        if (!msg_shutdown) {
                            pfd.revents = 0;
                            co::reset_writable(socket_->native_handle());
                            DebugPrint(dbg_session_alive, "goSend enter poll(timeout=%d)", timeo);
                            int res = ::poll(&pfd, 1, timeo);
                            DebugPrint(dbg_session_alive, "goSend exit poll(timeout=%d)", timeo);
                            if (res < 0) {
                                if (errno == EINTR) goto retry_poll;
                                ec = boost_ec(errno, boost::system::system_category());
                            } else if (pfd.revents == POLLOUT) {
                                goto retry_write;
                            }
                        }
                    } else {
                        ec = boost_ec(errno, boost::system::system_category());
                    }
                } else {
                    n = (std::size_t)nbytes;
                }

//                std::size_t n = socket_->write_some(buffers, ec);
                DebugPrint(dbg_no_delay, "write_some (bytes=%lu) returns %lu. is_error:%d",
                        write_bytes, n, !!ec);
                if (ec) {
                    SetCloseEc(ec);
                    DebugPrint(dbg_session_alive, "TcpSession send shutdown with write. %s:%d. error %d:%s",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port(),
                            ec.value(), ec.message().c_str());
                    ShutdownSend();
                    return ;
                }

                // Remove sended msg. restore send-half and non-send msgs.
                it = msg_send_list_.begin();
                while (it != msg_send_list_.end() && n > 0) {
                    auto &msg = *it;
                    std::size_t msg_capa = msg->buf.size() - msg->pos;
                    if (msg_capa <= n) {
                        msg->Done(boost_ec());
                        it = msg_send_list_.erase(it);
                        n -= msg_capa;
                    } else if (msg_capa > n) {
                        msg->pos += n;
                        msg->send_half = true;
                        break;
                    }
                }
            }
        };
    }

    void TcpSession::SetCloseEc(boost_ec const& ec)
    {
        if (close_ec_mutex_.try_lock() && !close_ec_)
            close_ec_ = ec;
    }

    void TcpSession::OnClose()
    {
        if (!closed_.try_lock()) return ;

        DebugPrint(dbg_session_alive, "TcpSession close %s:%d",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
        socket_->close();

        for (;;) {
            boost::shared_ptr<Msg> msg;
            if (!msg_chan_.TryPop(msg)) break;
            msg->Done(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
        }

        for (auto &msg : msg_send_list_)
            msg->Done(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
        msg_send_list_.clear();

        // 这个回调会减少TcpSession的引用计数, 进入析构. 因此一定要放在函数尾部。
        // 并且前面不能用Guard类操作.
        if (this->opt_.disconnect_cb_)
            this->opt_.disconnect_cb_(GetSession(), close_ec_);
    }

    void TcpSession::SendNoDelay(Buffer && buf, SndCb const& cb)
    {
        if (buf.empty()) {
            if (cb)
                cb(boost_ec());
            return ;
        }

        if (recv_shutdown_ || send_shutdown_ || initiative_shutdown_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        if (socket_->type() == tcp_socket_type_t::ssl) {
            Send(std::move(buf), cb);
            return ;
        }

        std::unique_lock<co::LFLock> send_token(send_mtx_, std::defer_lock);
        if (!send_token.try_lock()) {
            Send(std::move(buf), cb);
            return ;
        }

        if (sending_) {
            Send(std::move(buf), cb);
            return ;
        }

        ssize_t written = ::write_f(socket_->native_handle(), buf.data(), buf.size());
        if (written <= 0) {
            // send error.
            send_token.unlock();
            Send(std::move(buf), cb);
            return ;
        } else if (written >= (ssize_t)buf.size()) {
            // all bytes sended.
            send_token.unlock();
            if (cb)
                cb(boost_ec());
            return ;
        } else {
            // half sended, locked still.
            auto msg = boost::make_shared<Msg>(++msg_id_, cb);
            msg->buf.swap(buf);
            msg->pos = written;
            msg->send_half = true;
            if (opt_.sndtimeo_) {
                msg->tid = co_timer_add(std::chrono::milliseconds(opt_.sndtimeo_),
                        [=]{
                            msg->timeout = true;
                        });
            }
            // 放到队列头
            msg_send_list_.push_front(msg);
            sending_ = true;
        }
    }
    void TcpSession::SendNoDelay(const void* data, size_t bytes, SndCb const& cb)
    {
        if (!data || !bytes) {
            if (cb)
                cb(boost_ec());
            return ;
        }

        if (recv_shutdown_ || send_shutdown_ || initiative_shutdown_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        if (socket_->type() == tcp_socket_type_t::ssl) {
            Send(data, bytes, cb);
            return ;
        }

        std::unique_lock<co::LFLock> send_token(send_mtx_, std::defer_lock);
        if (!send_token.try_lock()) {
            DebugPrint(dbg_no_delay, "Send token try_lock failed.");
            Send(data, bytes, cb);
            return ;
        }

        if (sending_) {
            DebugPrint(dbg_no_delay, "in sending.");
            Send(data, bytes, cb);
            return ;
        }

        ssize_t written = ::write_f(socket_->native_handle(), data, bytes);
        DebugPrint(dbg_no_delay, "Send no delay(bytes=%lu) returns %ld.",
                bytes, written);
        if (written <= 0) {
            // send error.
            send_token.unlock();
            Send(data, bytes, cb);
            return ;
        } else if (written >= (ssize_t)bytes) {
            // all bytes sended.
            send_token.unlock();
            if (cb)
                cb(boost_ec());
            return ;
        } else {
            // half sended, locked still.
            Buffer buf((char*)data + written, (char*)data + bytes);
            auto msg = boost::make_shared<Msg>(++msg_id_, cb);
            msg->buf.swap(buf);
            msg->pos = 0;
            msg->send_half = true;
            if (opt_.sndtimeo_) {
                msg->tid = co_timer_add(std::chrono::milliseconds(opt_.sndtimeo_),
                        [=]{
                            msg->timeout = true;
                        });
            }
            // 放到队列头
            msg_send_list_.push_front(msg);
            sending_ = true;
        }
    }

    void TcpSession::Send(Buffer && buf, SndCb const& cb)
    {
        if (buf.empty()) {
            if (cb)
                cb(boost_ec());
            return ;
        }

        if (recv_shutdown_ || send_shutdown_ || initiative_shutdown_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        auto msg = boost::make_shared<Msg>(++msg_id_, cb);
        msg->buf.swap(buf);
        if (opt_.sndtimeo_) {
            msg->tid = co_timer_add(std::chrono::milliseconds(opt_.sndtimeo_),
                    [=]{
                        msg->timeout = true;
                    });
        }

        if (!msg_chan_.TryPush(msg)) {
            msg->Done(MakeNetworkErrorCode(eNetworkErrorCode::ec_send_overflow));
            return ;
        }
    }
    void TcpSession::Send(const void* data, size_t bytes, SndCb const& cb)
    {
        Buffer buf(bytes);
        memcpy(&buf[0], data, bytes);
        Send(std::move(buf), cb);
    }

    boost_ec TcpSession::SetSocketOptNoDelay(bool is_nodelay)
    {
        boost_ec ec;
        boost::asio::ip::tcp::no_delay opt_delay(is_nodelay);
        socket_->native_socket().set_option(opt_delay, ec);
        return ec;
    }

    bool TcpSession::IsEstab()
    {
        return !close_ec_;
    }

    endpoint TcpSession::LocalAddr()
    {
        return local_addr_;
    }
    endpoint TcpSession::RemoteAddr()
    {
        return remote_addr_;
    }
    std::size_t TcpSession::GetSendQueueSize()
    {
        return msg_chan_.size();
    }

    SessionEntry TcpSession::GetSession()
    {
        return this->shared_from_this();
    }

    boost_ec TcpServer::goStartBeforeFork(endpoint addr)
    {
        try {
            acceptor_.reset(new tcp::acceptor(GetTcpIoService()));
            acceptor_->open(tcp::endpoint(addr).protocol());
            acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor_->bind(addr);
            acceptor_->listen(opt_.listen_backlog_);
            local_addr_ = endpoint(acceptor_->local_endpoint(), addr.ext());
        } catch (boost::system::system_error& e) {
            return e.code();
        }
        return boost_ec();
    }
    void TcpServer::goStartAfterFork()
    {
        auto this_ptr = this->shared_from_this();
        go_dispatch(egod_robin) [this_ptr] {
            this_ptr->Accept();
        };
    }

    boost_ec TcpServer::goStart(endpoint addr)
    {
        boost_ec ec = goStartBeforeFork(addr);
        if (ec) return ec;

        goStartAfterFork();
        return ec;
    }
    void TcpServer::Shutdown(bool immediately)
    {
        shutdown_ = true;
        if (acceptor_)
            shutdown(acceptor_->native_handle(), socket_base::shutdown_both);

        std::lock_guard<co_mutex> lock(sessions_mutex_);
        for (auto &v : sessions_)
            v.second->Shutdown(immediately);
    }
    void TcpServer::Accept()
    {
        auto this_ptr = this->shared_from_this();
        tcp_context ctx(tcp_socket::create_tcp_context(opt_.ssl_option_));

        for (;;)
        {
            shared_ptr<tcp_socket> s(new tcp_socket(GetTcpIoService(),
                        local_addr_.proto() == proto_type::tcp ? tcp_socket_type_t::tcp : tcp_socket_type_t::ssl,
                        ctx));

            // aspect before accept
            if (opt_.accept_aspect_.before_aspect)
                opt_.accept_aspect_.before_aspect();

            boost_ec ec;
            acceptor_->accept(s->native_socket(), ec);

            // aspect after accept
            if (opt_.accept_aspect_.after_aspect)
                opt_.accept_aspect_.after_aspect();

            if (ec) {
                if (shutdown_) {
                    boost_ec ignore_ec;
                    acceptor_->close(ignore_ec);
                    DebugPrint(dbg_accept_debug, "accept end");
                    return ;
                }

                DebugPrint(dbg_accept_error, "accept error %d:%s",
                        ec.value(), ec.message().c_str());
                co_yield;
                continue;
            }

            DebugPrint(dbg_accept_debug, "accept from %s:%d",
                    s->native_socket().remote_endpoint().address().to_string().c_str(),
                    s->native_socket().remote_endpoint().port());

            go_dispatch(egod_robin) [s, this_ptr, this] {
                boost_ec ec = s->handshake(handshake_type_t::server);
                if (ec) return ;

                shared_ptr<TcpSession> sess(new TcpSession(s, this->shared_from_this(), opt_, local_addr_.ext()));

                {
                    std::unique_lock<co_mutex> lock(sessions_mutex_);
                    if (shutdown_) {
                        lock.unlock();
                        sess->Shutdown();
                        return;
                    } else if (sessions_.size() >= opt_.max_connection_) {
                        lock.unlock();
                        sess->Shutdown();
                        return;
                    } else {
                        sessions_[sess->GetSession()] = sess;
                    }
                }

                sess->SetSndTimeout(opt_.sndtimeo_)
                    .SetConnectedCb(opt_.connect_cb_)
                    .SetReceiveCb(opt_.receive_cb_)
                    .SetDisconnectedCb(boost::bind(&TcpServer::OnSessionClose, this, _1, _2))
                    .goStart();
            };
        }
    }

    void TcpServer::OnSessionClose(::network::SessionEntry id, boost_ec const& ec)
    {
        // 后面的erase会导致TcpServer引用计数减少, 可能进入析构函数.
        // 此处引用计数guard以保证析构函数结束后不再执行解锁操作.
        auto self = this->shared_from_this();   

        if (opt_.disconnect_cb_)
            opt_.disconnect_cb_(id, ec);

        std::lock_guard<co_mutex> lock(sessions_mutex_);
        sessions_.erase(id);
    }

    endpoint TcpServer::LocalAddr()
    {
        return local_addr_;
    }

    std::size_t TcpServer::SessionCount()
    {
        return sessions_.size();
    }

    boost_ec TcpClient::Connect(endpoint addr)
    {
        if (sess_ && sess_->IsEstab()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_estab);
        std::unique_lock<co_mutex> lock(connect_mtx_, std::defer_lock);
        if (!lock.try_lock()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_connecting);

        tcp_context ctx(tcp_socket::create_tcp_context(opt_.ssl_option_));
        shared_ptr<tcp_socket> s(new tcp_socket(GetTcpIoService(),
                    addr.proto() == proto_type::tcp ? tcp_socket_type_t::tcp : tcp_socket_type_t::ssl,
                    ctx));
        boost_ec ec;
        s->native_socket().connect(addr, ec);
        if (ec) return ec;

        ec = s->handshake(handshake_type_t::client);
        if (ec) return ec;

        sess_.reset(new TcpSession(s, this->shared_from_this(), opt_, addr.ext()));
        sess_->SetSndTimeout(opt_.sndtimeo_)
            .SetConnectedCb(opt_.connect_cb_)
            .SetReceiveCb(opt_.receive_cb_)
            .SetDisconnectedCb(boost::bind(&TcpClient::OnSessionClose, this, _1, _2));

        auto sess = sess_;
        go_dispatch(egod_robin) [sess] {
            sess->goStart();
        };
        return boost_ec();
    }
    SessionEntry TcpClient::GetSession()
    {
        return sess_ ? sess_->GetSession() : SessionEntry();
    }

    void TcpClient::OnSessionClose(::network::SessionEntry id, boost_ec const& ec)
    {
        // 后面的reset会导致TcpClient引用计数减少, 可能进入析构函数.
        // 此处引用计数guard以保证析构函数结束后不再执行sess_.reset后续逻辑。
        auto self = this->shared_from_this();

        if (opt_.disconnect_cb_)
            opt_.disconnect_cb_(id, ec);
        sess_.reset();
    }

} //namespace tcp_detail
} //namespace network

