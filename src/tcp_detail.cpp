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

    TcpSession::TcpSession(shared_ptr<tcp::socket> s, shared_ptr<LifeHolder> holder, uint32_t max_pack_size)
        : socket_(s), holder_(holder), recv_buf_(max_pack_size), msg_chan_(-1), shutdown_ref_{2}
    {
        boost_ec ignore_ec;
        local_addr_ = s->local_endpoint(ignore_ec);
        remote_addr_ = s->remote_endpoint(ignore_ec);
    }

    TcpSession::~TcpSession()
    {
        DebugPrint(dbg_session_alive, "TcpSession destruct %s:%d",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
    }

    void TcpSession::goStart()
    {
        auto this_ptr = this->shared_from_this();
        if (opt_.connect_cb_)
            opt_.connect_cb_(GetId());

        go [=] {
            auto holder = this_ptr;
            goReceive();
            goSend();
        };

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
        go [=]{
            auto holder = this_ptr;
            size_t pos = 0;
            for (;;)
            {
                boost_ec ec;
                std::size_t n = 0;
                if (pos >= recv_buf_.size()) {
                    ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_recv_overflow);
                } else {
                    n = socket_->read_some(buffer(&recv_buf_[pos], recv_buf_.size() - pos), ec);
                }

                if (!ec) {
                    if(n > 0) {
//                        printf("receive %u bytes: %s\n", (unsigned)n, to_hex(&recv_buf_[pos], n).c_str());
                        if (this->opt_.receive_cb_) {
                            size_t consume = this->opt_.receive_cb_(GetId(), recv_buf_.data(), n + pos);
                            if (consume == (size_t)-1)
                                ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_data_parse_error);
                            else {
                                assert(consume <= n + pos);
                                pos = n + pos - consume;
                                if (pos > 0)
                                    memcpy(&recv_buf_[0], &recv_buf_[consume], pos);
                            }
                        } else {
                            pos += n;
                        }
                    }
                }

                if (ec) {
                    SetCloseEc(ec);
                    boost_ec ignore_ec;
                    socket_->shutdown(socket_base::shutdown_both, ignore_ec);
                    if (--shutdown_ref_ == 0)
                        OnClose();

                    DebugPrint(dbg_session_alive, "TcpSession receive shutdown %s:%d",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port());
                    return ;
                } 
            }
        };
    }

    void TcpSession::goSend()
    {
        auto this_ptr = this->shared_from_this();
        go [=]{
            auto holder = this_ptr;
            for (;;)
            {
                if (shutdown_ref_ == 1) {
                    --shutdown_ref_;
                    DebugPrint(dbg_session_alive, "TcpSession send shutdown with cond %s:%d",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port());
                    OnClose();
                    return ;
                }

                static const int c_multi = 1024;

                for (int i = 0; i < c_multi; ++i)
                {
                    boost::shared_ptr<Msg> msg;
                    if (!msg_chan_.TryPop(msg)) break;
                    if (msg->timeout) {
                        if (msg->cb)
                            msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_timeout));
                    } else
                        msg_send_list_.push_back(msg);
                }

                if (msg_send_list_.empty()) {
                    co_yield;
                    continue;
                }

                // Make buffers
                std::vector<const_buffer> buffers(std::min<int>(msg_send_list_.size(), c_multi));
                int i = 0;
                auto it = msg_send_list_.begin();
                while (it != msg_send_list_.end())
                {
                    auto &msg = *it;
                    if (msg->timeout && !msg->send_half) {
                        if (msg->cb)
                            msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_timeout));
                        it = msg_send_list_.erase(it);
                        continue;
                    }

                    if (i >= c_multi) break;
                    buffers[i] = buffer(&msg->buf[msg->pos], msg->buf.size() - msg->pos);
                    ++it;
                    ++i;
                }
                buffers.resize(i);

                if (buffers.empty()) {
                    co_yield;
                    continue;
                }

                // Send Once
                boost_ec ec;
                std::size_t n = socket_->write_some(buffers, ec);
                if (ec) {
                    SetCloseEc(ec);

                    boost_ec ignore_ec;
                    socket_->shutdown(socket_base::shutdown_both, ignore_ec);
                    if (--shutdown_ref_ == 0) {
                        OnClose();
                    }

                    DebugPrint(dbg_session_alive, "TcpSession send shutdown with write_some %s:%d",
                            remote_addr_.address().to_string().c_str(), remote_addr_.port());
                    return ;
                }

                // Remove sended msg. restore send-half and non-send msgs.
                it = msg_send_list_.begin();
                while (it != msg_send_list_.end() && n > 0) {
                    auto &msg = *it;
                    std::size_t msg_capa = msg->buf.size() - msg->pos;
                    if (msg_capa <= n) {
                        if (msg->cb)
                            msg->cb(boost_ec());
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
        std::lock_guard<co_mutex> lock(close_ec_mutex_);
        if (!close_ec_)
            close_ec_ = ec;
    }

    void TcpSession::OnClose()
    {
        DebugPrint(dbg_session_alive, "TcpSession close %s:%d",
                remote_addr_.address().to_string().c_str(), remote_addr_.port());
        boost_ec ignore_ec;
        socket_->close(ignore_ec);

        for (;;) {
            boost::shared_ptr<Msg> msg;
            if (!msg_chan_.TryPop(msg)) break;
            if (msg->cb)
                msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
        }

        for (auto &msg : msg_send_list_)
            if (msg->cb)
                msg->cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
        msg_send_list_.clear();

        if (this->opt_.disconnect_cb_)
            this->opt_.disconnect_cb_(GetId(), close_ec_);
    }

    void TcpSession::Send(Buffer && buf, SndCb const& cb)
    {
        if (shutdown_ref_ < 2) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        if (buf.empty()) {
            if (cb)
                cb(boost_ec());
            return ;
        }

        auto msg = boost::make_shared<Msg>(++msg_id_, cb);
        msg->buf.swap(buf);
        if (opt_.sndtimeo_) {
            auto this_ptr = this->shared_from_this();
            msg->tid = co_timer_add(std::chrono::milliseconds(opt_.sndtimeo_),
                    [=]{
                        msg->timeout = true;
                    });
        }

        if (!msg_chan_.TryPush(msg)) {
            if (msg->tid)
                co_timer_cancel(msg->tid);

            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_send_overflow));
            return ;
        }
    }
    void TcpSession::Send(const void* data, size_t bytes, SndCb const& cb)
    {
        Buffer buf(bytes);
        memcpy(&buf[0], data, bytes);
        Send(std::move(buf), cb);
    }

    void TcpSession::Shutdown()
    {
        SetCloseEc(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
        boost_ec ignore_ec;
        socket_->shutdown(socket_base::shutdown_both, ignore_ec);
    }

    bool TcpSession::IsEstab()
    {
        return !close_ec_;
    }

    tcp_sess_id_t TcpSession::GetId()
    {
        return this->shared_from_this();
    }

    boost_ec TcpServerImpl::goStart(endpoint addr)
    {
        try {
            local_addr_ = addr;
            acceptor_.reset(new tcp::acceptor(GetTcpIoService(), local_addr_, true));
        } catch (boost::system::system_error& e)
        {
            return e.code();
        }

        auto this_ptr = this->shared_from_this();
        go [this_ptr] {
            this_ptr->Accept();
        };
        return boost_ec();
    }

    void TcpServerImpl::ShutdownAll()
    {
        std::lock_guard<co_mutex> lock(sessions_mutex_);
        for (auto &v : sessions_)
            v.second->Shutdown();
    }
    void TcpServerImpl::Shutdown()
    {
        shutdown_ = true;
        if (acceptor_)
            shutdown(acceptor_->native_handle(), socket_base::shutdown_both);
        ShutdownAll();
    }
    void TcpServerImpl::Accept()
    {
        for (;;)
        {
            shared_ptr<tcp::socket> s(new tcp::socket(GetTcpIoService()));
            boost_ec ec;
            acceptor_->accept(*s, ec);
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
                    s->remote_endpoint().address().to_string().c_str(),
                    s->remote_endpoint().port());

            shared_ptr<TcpSession> sess(new TcpSession(s, this->shared_from_this(), opt_.max_pack_size_));

            {
                std::lock_guard<co_mutex> lock(sessions_mutex_);
                sessions_[sess->GetId()] = sess;
            }

            sess->SetSndTimeout(opt_.sndtimeo_)
                .SetConnectedCb(opt_.connect_cb_)
                .SetReceiveCb(opt_.receive_cb_)
                .SetDisconnectedCb(boost::bind(&TcpServerImpl::OnSessionClose, this, _1, _2))
                .goStart();
        }
    }

    void TcpServerImpl::OnSessionClose(::network::SessionId id, boost_ec const& ec)
    {
        if (opt_.disconnect_cb_)
            opt_.disconnect_cb_(id, ec);

        std::lock_guard<co_mutex> lock(sessions_mutex_);
        sessions_.erase(id);
    }

    tcp::endpoint TcpServerImpl::LocalAddr()
    {
        return local_addr_;
    }


    boost_ec TcpClientImpl::Connect(endpoint addr)
    {
        if (sess_ && sess_->IsEstab()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_estab);
        std::unique_lock<co_mutex> lock(connect_mtx_, std::defer_lock);
        if (!lock.try_lock()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_connecting);

        shared_ptr<tcp::socket> s(new tcp::socket(GetTcpIoService()));
        boost_ec ec;
        s->connect(addr, ec);
        if (ec)
            return ec;

        sess_.reset(new TcpSession(s, this->shared_from_this(), opt_.max_pack_size_));
        sess_->SetSndTimeout(opt_.sndtimeo_)
            .SetConnectedCb(opt_.connect_cb_)
            .SetReceiveCb(opt_.receive_cb_)
            .SetDisconnectedCb(boost::bind(&TcpClientImpl::OnSessionClose, this, _1, _2))
            .goStart();
        return boost_ec();
    }
    tcp_sess_id_t TcpClientImpl::GetSessId()
    {
        return sess_ ? sess_->GetId() : tcp_sess_id_t();
    }

    void TcpClientImpl::OnSessionClose(::network::SessionId id, boost_ec const& ec)
    {
        if (opt_.disconnect_cb_)
            opt_.disconnect_cb_(id, ec);
        sess_.reset();
    }

    TcpClient::~TcpClient()
    {
        auto sess = impl_->GetSessId();
        if (sess)
            sess->Shutdown();
    }

} //namespace tcp_detail
} //namespace network

