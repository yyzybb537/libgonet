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
        typedef Protocol::sess_id_t sess_id_t;

        Server();
        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec goStart(std::string const& url);
        void Send(sess_id_t id, Buffer && buf, SndCb const& cb = NULL);
        void Send(sess_id_t id, const void* data, size_t bytes, SndCb const& cb = NULL);
        endpoint RemoteAddr(sess_id_t id);
        endpoint LocalAddr();
        void Shutdown(sess_id_t id);
        void Shutdown();
        ProtocolRef GetProtocol();

    private:
        boost::shared_ptr<Protocol*> protocol_;
        boost::shared_ptr<ServerBase> impl_;
        boost::shared_ptr<endpoint> local_addr_;
    };

    class Client : public Options<Client>
    {
    public:
        typedef Protocol::sess_id_t sess_id_t;

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
        SessionId GetSessId();
        ProtocolRef GetProtocol();

    private:
        boost::shared_ptr<Protocol*> protocol_;
        boost::shared_ptr<ClientBase> impl_;
        boost::shared_ptr<co_mutex> connect_mtx_;
        boost::shared_ptr<endpoint> local_addr_;
    };

} //namespace network
