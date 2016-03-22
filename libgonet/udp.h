#pragma once
#include "udp_detail.h"
#include "abstract.h"

namespace network {

class udp : public Protocol
{
public:
    typedef Protocol::endpoint endpoint;
    typedef udp_detail::UdpServer server;
    typedef udp_detail::UdpClient client;

    udp();
    virtual boost::shared_ptr<ServerBase> CreateServer();
    virtual boost::shared_ptr<ClientBase> CreateClient();
    
    static udp* instance();
};

}//namespace network
