#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include "error.h"
#include <libgo/coroutine.h>

namespace network {

    static const uint64_t dbg_accept_error           = co::dbg_sys_max;
    static const uint64_t dbg_accept_debug           = co::dbg_sys_max << 1;
    static const uint64_t dbg_session_alive          = co::dbg_sys_max << 2;
    static const uint64_t dbg_network_max            = dbg_session_alive;

    enum class proto_type {
        unkown,
        tcp,
        udp,
        tls,
        http,
        https,
        zk,
    };
    proto_type str2proto(std::string const& s);
    std::string proto2str(proto_type proto);

    struct Protocol;
    struct endpoint : public ::boost::asio::ip::basic_endpoint<Protocol>
    {
        typedef ::boost::asio::ip::basic_endpoint<Protocol> base_t;

        endpoint() {}
        endpoint(const endpoint&) = default;
        endpoint(endpoint&&) = default;
        endpoint& operator=(const endpoint&) = default;
        endpoint& operator=(endpoint&&) = default;

        using base_t::base_t;

        template <typename Proto>
            explicit endpoint(::boost::asio::ip::basic_endpoint<Proto> const& ep)
            : base_t(ep.addres(), ep.port())
            {
            }

        operator ::boost::asio::ip::tcp::endpoint() const
        {
            return ::boost::asio::ip::tcp::endpoint(address(), port()); 
        }

        operator ::boost::asio::ip::udp::endpoint() const
        {
            return ::boost::asio::ip::udp::endpoint(address(), port()); 
        }

        std::string to_string(boost_ec & ec) const;

        // @url:
        //  tcp://127.0.0.1:3030
        //  udp://127.0.0.1:3030
        //  tls://127.0.0.1:3030
        //  http://127.0.0.1
        //  http://127.0.0.1:3030
        //  http://127.0.0.1:3030/route/index.html
        //  https
        //  zk://127.0.0.1:2181,192.168.1.10:2181/zk_path/node
        static endpoint from_string(std::string const& url, boost_ec & ec);

        proto_type proto_ = proto_type::unkown;
        std::string path_;
    };

    struct OptionsBase;
    struct SessionIdBase
    {
        virtual ~SessionIdBase() {}
    };
    typedef boost::shared_ptr<SessionIdBase> SessionId;
    struct ServerBase
    {
        virtual ~ServerBase() {}
        virtual boost_ec goStart(endpoint addr) = 0;
        virtual void Shutdown() = 0;
        virtual OptionsBase* GetOptions() = 0;
    };
    struct ClientBase
    {
        virtual ~ClientBase() {}
        virtual boost_ec Connect(endpoint addr) = 0;
        virtual SessionId GetSessId() = 0;
        virtual OptionsBase* GetOptions() = 0;
    };

    typedef std::vector<char> Buffer;
    typedef boost::function<void(boost_ec const&)> SndCb;
    typedef boost::function<size_t(SessionId, const char* data, size_t bytes)> ReceiveCb;

    // ----- tcp protocol effect only ------
    typedef boost::function<void(SessionId)> ConnectedCb;
    typedef boost::function<void(SessionId, boost_ec const&)> DisconnectedCb;
    // -------------------------------------

    struct Protocol
    {
        typedef ::network::SessionId sess_id_t;
        typedef ::network::endpoint endpoint;
        virtual ~Protocol() {}

        static Protocol v4();
        static Protocol v6();
        int type() const;
        int protocol() const;
        int family() const;

        virtual void Send(sess_id_t id, Buffer && buf, SndCb const& cb = NULL) {}
        virtual void Send(sess_id_t id, const void* data, size_t bytes, SndCb const& cb = NULL) {}
        virtual void Shutdown(sess_id_t id) {}
        virtual bool IsEstab(sess_id_t id) { return true; }
        virtual endpoint LocalAddr(sess_id_t id) { return endpoint(); }
        virtual endpoint RemoteAddr(sess_id_t id) { return endpoint(); }
        virtual boost::shared_ptr<ServerBase> CreateServer() {return NULL;}
        virtual boost::shared_ptr<ClientBase> CreateClient() {return NULL;}

    protected:
        explicit Protocol(int family, proto_type proto = proto_type::tcp)
            : family_(family), proto_(proto) {}

        int family_;
        proto_type proto_;
    };

#ifdef _DEBUG
# define NETWORK_UPPER_CAST(dest) dynamic_cast<dest*>
#else
# define NETWORK_UPPER_CAST(dest) (dest*)
#endif

} //namespace network
