#pragma once
#include "udp_detail.h"
#include "abstract.h"

namespace network {

class udp : public Protocol
{
public:
    typedef Protocol::sess_id_t sess_id_t;
    typedef Protocol::endpoint endpoint;
    typedef udp_detail::UdpServer server;
    typedef udp_detail::UdpClient client;

    udp();
    virtual void Send(sess_id_t id, Buffer && buf, SndCb const& cb = NULL) override;
    virtual void Send(sess_id_t id, const void* data, size_t bytes, SndCb const& cb = NULL) override;
    virtual void Shutdown(sess_id_t id) override;
    virtual bool IsEstab(sess_id_t id) override;
    virtual endpoint LocalAddr(sess_id_t id) override;
    virtual endpoint RemoteAddr(sess_id_t id) override;
    virtual boost::shared_ptr<ServerBase> CreateServer();
    virtual boost::shared_ptr<ClientBase> CreateClient();
    
    static udp* instance();
};

}//namespace network
