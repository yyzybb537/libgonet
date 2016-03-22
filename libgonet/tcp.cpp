#include "tcp.h"
#include <boost/make_shared.hpp>

namespace network {

    tcp::tcp()
        : Protocol(::boost::asio::ip::tcp::v4().family(), proto_type::tcp)
    {}

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
