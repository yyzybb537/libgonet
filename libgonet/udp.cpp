#include "udp.h"
#include <boost/make_shared.hpp>

namespace network {

    udp::udp()
        : Protocol(::boost::asio::ip::udp::v4().family(), proto_type::udp)
    {}

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
