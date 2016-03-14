#include "udp.h"
#include <boost/make_shared.hpp>

namespace network {

    udp::udp()
        : Protocol(::boost::asio::ip::udp::v4().family(), proto_type::udp)
    {}

    void udp::Send(sess_id_t id, Buffer && buf, SndCb const& cb)
    {
        auto udp_id = NETWORK_UPPER_CAST(udp_detail::_udp_sess_id_t)(id.get());
        if (!udp_id) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }
        boost_ec ec = udp_id->udp_point->Send(udp_id->remote_addr, &buf[0], buf.size());
        if (cb) cb(ec);
    }
    void udp::Send(sess_id_t id, const void* data, size_t bytes, SndCb const& cb)
    {
        auto udp_id = NETWORK_UPPER_CAST(udp_detail::_udp_sess_id_t)(id.get());
        if (!udp_id) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }
        boost_ec ec = udp_id->udp_point->Send(udp_id->remote_addr, data, bytes);
        if (cb) cb(ec);
    }
    void udp::Shutdown(sess_id_t id)
    {
        auto udp_id = NETWORK_UPPER_CAST(udp_detail::_udp_sess_id_t)(id.get());
        if (!udp_id) return ;
        udp_id->udp_point->Shutdown();
    }
    bool udp::IsEstab(sess_id_t id)
    {
        (void)id;
        return true;
    }
    udp::endpoint udp::LocalAddr(sess_id_t id)
    {
        auto udp_id = NETWORK_UPPER_CAST(udp_detail::_udp_sess_id_t)(id.get());
        if (!udp_id) return udp::endpoint();
        auto asio_endpoint = udp_id->udp_point->LocalAddr();
        return udp::endpoint(asio_endpoint.address(), asio_endpoint.port());
    }
    udp::endpoint udp::RemoteAddr(sess_id_t id)
    {
        auto udp_id = NETWORK_UPPER_CAST(udp_detail::_udp_sess_id_t)(id.get());
        if (!udp_id) return udp::endpoint();
        auto asio_endpoint = udp_id->remote_addr;
        return udp::endpoint(asio_endpoint.address(), asio_endpoint.port());
    }
    boost::shared_ptr<ServerBase> udp::CreateServer()
    {
        return boost::make_shared<server>();
    }
    boost::shared_ptr<ClientBase> udp::CreateClient()
    {
        return boost::make_shared<client>();
    }
    udp* udp::instance()
    {
        static udp obj;
        return &obj;
    }

} //namespace network
