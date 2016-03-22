#pragma once

#include "error.h"
#include "tcp.h"
#include "udp.h"

namespace network
{
    class Server : public Options<Server>
    {
    public:
        Server();
        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec goStart(std::string const& url);
        endpoint LocalAddr();
        void Shutdown();
        Protocol const& GetProtocol();

    private:
        Protocol* protocol_ = nullptr;
        boost::shared_ptr<ServerBase> impl_;
        boost::shared_ptr<endpoint> local_addr_;
    };

    class Client : public Options<Client>
    {
    public:
        Client();
        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec Connect(std::string const& url);
        void Send(Buffer && buf, SndCb const& cb = NULL);
        void Send(const void* data, size_t bytes, SndCb const& cb = NULL);
        void Shutdown();
        bool IsEstab();
        endpoint LocalAddr();
        endpoint RemoteAddr();
        SessionEntry GetSession();
        Protocol const& GetProtocol();

    private:
        Protocol* protocol_ = nullptr;
        boost::shared_ptr<ClientBase> impl_;
        boost::shared_ptr<co_mutex> connect_mtx_;
        boost::shared_ptr<endpoint> local_addr_;
    };

} //namespace network
