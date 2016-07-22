#include "udp_detail.h"
#include <boost/make_shared.hpp>

namespace network {
namespace udp_detail {

    void _udp_sess_id_t::SendNoDelay(Buffer && buf, SndCb const& cb)
    {
        Send(std::move(buf), cb);
    }
    void _udp_sess_id_t::SendNoDelay(const void* data, size_t bytes, SndCb const& cb)
    {
        Send(data, bytes, cb);
    }
    void _udp_sess_id_t::Send(Buffer && buf, const SndCb & cb)
    {
        if (!udp_point) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        boost_ec ec = udp_point->Send(remote_addr, buf.data(), buf.size());
        if (cb) cb(ec);
    }
    void _udp_sess_id_t::Send(const void * data, size_t bytes, const SndCb & cb)
    {
        if (!udp_point) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        boost_ec ec = udp_point->Send(remote_addr, data, bytes);
        if (cb) cb(ec);
    }
    bool _udp_sess_id_t::IsEstab()
    {
        return true;
    }
    void _udp_sess_id_t::Shutdown(bool)
    {
        return ;
    }
    endpoint _udp_sess_id_t::LocalAddr()
    {
        if (!udp_point) return endpoint();
        return endpoint(udp_point->LocalAddr(), proto_type::udp);
    }
    endpoint _udp_sess_id_t::RemoteAddr()
    {
        return endpoint(remote_addr, proto_type::udp);
    }
    std::size_t _udp_sess_id_t::GetSendQueueSize()
    {
        return 0;
    }

    UdpPoint::UdpPoint()
    {
        recv_buf_.resize(opt_.max_pack_size_);
        local_addr_ = endpoint(udp::endpoint(udp::v4(), 0));
    }

    io_service& UdpPoint::GetUdpIoService()
    {
        static io_service ios;
        return ios;
    }

    boost_ec UdpPoint::goStartBeforeFork(endpoint addr)
    {
        return goStart(addr);
    }
    boost_ec UdpPoint::goStart(endpoint local_endpoint)
    {
        if (init_) return MakeNetworkErrorCode(eNetworkErrorCode::ec_estab);

        try {
            socket_.reset(new udp::socket(GetUdpIoService(), udp::endpoint(local_endpoint)));
        } catch (boost::system::system_error& e) {
            return e.code();
        }

        boost_ec ignore_ec;
        local_addr_ = endpoint(socket_->local_endpoint(ignore_ec), local_endpoint.ext());
        init_ = true;
        auto this_ptr = this->shared_from_this();
        go_dispatch(egod_robin) [this_ptr] {
            this_ptr->DoRecv();
        };
        return boost_ec();
    }
    void UdpPoint::Shutdown(bool)
    {
        if (!init_) return ;
        if (shutdown_) return ;
        shutdown_ = true;
        boost_ec ignore_ec;
        socket_->shutdown(socket_base::shutdown_both, ignore_ec);
        socket_->close(ignore_ec);
        DebugPrint(dbg_session_alive, "udp::Shutdown");
        recv_shutdown_channel_ >> nullptr;
        if (opt_.disconnect_cb_)
            opt_.disconnect_cb_(GetSession(), MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
    }
    void UdpPoint::OnSetMaxPackSize()
    {
        recv_buf_.resize(opt_.max_pack_size_);
    }
    void UdpPoint::DoRecv()
    {
        for (;;)
        {
            boost_ec ec;
            udp::endpoint from_addr;
            std::size_t addrlen = from_addr.size();
            ::boost::asio::detail::socket_ops::buf b{&recv_buf_[0], recv_buf_.size()};
            ssize_t n = ::boost::asio::detail::socket_ops::recvfrom(socket_->native_handle(),
                    &b, 1, 0, from_addr.data(), &addrlen, ec);
            if (!ec && n > 0) {
                from_addr.resize(addrlen);

                if (opt_.receive_cb_) {
                    udp_sess_id_t sess_id = boost::make_shared<_udp_sess_id_t>(
                            this->shared_from_this(), endpoint(from_addr, local_addr_.ext()));
                    opt_.receive_cb_(sess_id, &recv_buf_[0], n);
                }
            }

            if (shutdown_) break;
        }

        DebugPrint(dbg_session_alive, "udp::DoRecv exit");
        recv_shutdown_channel_ << nullptr;
    }

    boost_ec UdpPoint::Send(std::string const& host, uint16_t port, const void* data, std::size_t bytes)
    {
        endpoint dst(address::from_string(host), port);
        return Send(dst, data, bytes);
    }
    boost_ec UdpPoint::Send(endpoint destition, const void* data, std::size_t bytes)
    {
        boost_ec ec;
        if (!init_) {
            ec = goStart(local_addr_);
            if (ec) return ec;
        }
        std::size_t n = socket_->send_to(buffer(data, bytes), destition, 0, ec);
        if (ec) return ec;
        if (n < bytes) return MakeNetworkErrorCode(eNetworkErrorCode::ec_half);
        return boost_ec();
    }
    boost_ec UdpPoint::Connect(endpoint addr)
    {
        boost_ec ec;
        if (!init_) {
            ec = goStart(local_addr_);
            if (ec) return ec;
        }
        socket_->connect(addr, ec);
        if (!ec) {
            remote_addr_ = addr;

            if (opt_.connect_cb_)
                opt_.connect_cb_(GetSession());
        }
        return ec;
    }
    boost_ec UdpPoint::Send(const void* data, size_t bytes)
    {
        boost_ec ec;
        if (!init_) {
            ec = goStart(local_addr_);
            if (ec) return ec;
        }
        std::size_t n = socket_->send(buffer(data, bytes), 0, ec);
        if (ec) return ec;
        if (n < bytes) return MakeNetworkErrorCode(eNetworkErrorCode::ec_half);
        return boost_ec();
    }
    endpoint UdpPoint::LocalAddr()
    {
        return local_addr_;
    }
    endpoint UdpPoint::RemoteAddr()
    {
        return remote_addr_;
    }
    SessionEntry UdpPoint::GetSession()
    {
        return boost::make_shared<_udp_sess_id_t>(this->shared_from_this(), remote_addr_);
    }

} //namespace udp_detail
} //namespace network

