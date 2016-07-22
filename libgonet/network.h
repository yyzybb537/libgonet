#pragma once

#include "config.h"
#include "error.h"
#include "tcp.h"
#include "udp.h"

namespace network
{
    class Server : public Options<Server>
    {
    public:
        Server();
        ~Server();

        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec goStart(std::string const& url);
        endpoint LocalAddr();
        void Shutdown(bool immediately = true);
        Protocol const& GetProtocol();

        boost_ec goStartBeforeFork(std::string const& url);
        void goStartAfterFork();

    private:
        Protocol* protocol_ = nullptr;
        boost::shared_ptr<ServerBase> impl_;
        boost::shared_ptr<endpoint> local_addr_;
    };

    class Client : public Options<Client>
    {
    public:
        Client();
        ~Client();

        // @url:
        //    tcp://127.0.0.1:3030
        //    udp://127.0.0.1:3030
        boost_ec Connect(std::string const& url);
        void SendNoDelay(Buffer && buf, SndCb const& cb = NULL);
        void SendNoDelay(const void* data, size_t bytes, SndCb const& cb = NULL);
        void Send(Buffer && buf, SndCb const& cb = NULL);
        void Send(const void* data, size_t bytes, SndCb const& cb = NULL);
        void Shutdown(bool immediately = true);
        bool IsEstab();
        endpoint LocalAddr();
        endpoint RemoteAddr();
        SessionEntry GetSession();
        Protocol const& GetProtocol();

    private:
        Protocol* protocol_ = nullptr;
        boost::shared_ptr<ClientBase> impl_;
        boost::shared_ptr<co_mutex> connect_mtx_;
        boost::shared_ptr<endpoint> remote_addr_;
    };

} //namespace network
