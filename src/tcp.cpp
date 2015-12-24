#include "tcp.h"
#include <boost/make_shared.hpp>

namespace network {

    tcp::tcp()
        : Protocol(::boost::asio::ip::tcp::v4().family(), proto_type::tcp)
    {}

    void tcp::Send(sess_id_t id, Buffer && buf, SndCb const& cb)
    {
        auto tcp_id = NETWORK_UPPER_CAST(tcp_detail::TcpSession)(id.get());
        if (!tcp_id) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        tcp_id->Send(std::move(buf), cb);
    }
    void tcp::Send(sess_id_t id, const void* data, size_t bytes, SndCb const& cb)
    {
        auto tcp_id = NETWORK_UPPER_CAST(tcp_detail::TcpSession)(id.get());
        if (!tcp_id) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        tcp_id->Send(data, bytes, cb);
    }
    void tcp::Shutdown(sess_id_t id)
    {
        auto tcp_id = NETWORK_UPPER_CAST(tcp_detail::TcpSession)(id.get());
        if (tcp_id)
            tcp_id->Shutdown();
    }
    bool tcp::IsEstab(sess_id_t id)
    {
        auto tcp_id = NETWORK_UPPER_CAST(tcp_detail::TcpSession)(id.get());
        return tcp_id ? tcp_id->IsEstab() : false;
    }
    tcp::endpoint tcp::LocalAddr(sess_id_t id)
    {
        auto tcp_id = NETWORK_UPPER_CAST(tcp_detail::TcpSession)(id.get());
        auto asio_endpoint = tcp_id ? tcp_id->local_addr_ : tcp::endpoint();
        return endpoint(asio_endpoint.address(), asio_endpoint.port());
    }
    tcp::endpoint tcp::RemoteAddr(sess_id_t id)
    {
        auto tcp_id = NETWORK_UPPER_CAST(tcp_detail::TcpSession)(id.get());
        auto asio_endpoint = tcp_id ? tcp_id->remote_addr_ : tcp::endpoint();
        return endpoint(asio_endpoint.address(), asio_endpoint.port());
    }
    boost::shared_ptr<ServerBase> tcp::CreateServer()
    {
        return boost::make_shared<server>();
    }
    boost::shared_ptr<ClientBase> tcp::CreateClient()
    {
        return boost::make_shared<client>();
    }

    tcp* tcp::instance()
    {
        static tcp obj;
        return &obj;
    }

} //namespace network
