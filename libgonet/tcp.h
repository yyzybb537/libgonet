#pragma once
#include "config.h"
#include "tcp_detail.h"
#include "abstract.h"

namespace network {

class tcp : public Protocol
{
public:
    typedef Protocol::endpoint endpoint;
    typedef tcp_detail::TcpServer server;
    typedef tcp_detail::TcpClient client;

    tcp();
    virtual boost::shared_ptr<ServerBase> CreateServer();
    virtual boost::shared_ptr<ClientBase> CreateClient();

    static tcp* instance();
};

}//namespace network
