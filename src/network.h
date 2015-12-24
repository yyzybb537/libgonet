#pragma once

#include "error.h"
#include "tcp.h"
#include "udp.h"

namespace network {

    struct ProtocolRef
    {
        explicit operator bool() const
        {
            return proto_ && *proto_;
        }

        Protocol* operator->() const
        {
            return *proto_;
        }

    private:
        boost::shared_ptr<Protocol*> proto_;
        explicit ProtocolRef(boost::shared_ptr<Protocol*> proto);
        friend class Server;
        friend class Client;
    };

    class Server : public Options<Server>
    {
    public:
        Server();
        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec goStart(std::string const& url);
        void Shutdown();
        endpoint LocalAddr();
        ProtocolRef GetProtocol();

    private:
        boost::shared_ptr<Protocol*> protocol_;
        boost::shared_ptr<ServerBase> impl_;
        endpoint local_addr_;
    };

    class Client : public Options<Client>
    {
    public:
        Client();
        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec Connect(std::string const& url);
        void Send(const void* data, size_t bytes, SndCb cb = NULL);
        void Shutdown();
        endpoint LocalAddr();
        endpoint RemoteAddr();
        ProtocolRef GetProtocol();
        SessionId GetSessId();

    private:
        boost::shared_ptr<Protocol*> protocol_;
        boost::shared_ptr<ClientBase> impl_;
        boost::shared_ptr<co_mutex> connect_mtx_;
        endpoint local_addr_;
    };

} //namespace network
