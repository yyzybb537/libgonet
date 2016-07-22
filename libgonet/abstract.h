#pragma once
#include "config.h"
#include "error.h"

namespace network {

    static const uint64_t dbg_accept_error           = co::dbg_sys_max;
    static const uint64_t dbg_accept_debug           = co::dbg_sys_max << 1;
    static const uint64_t dbg_session_alive          = co::dbg_sys_max << 2;
    static const uint64_t dbg_no_delay               = co::dbg_sys_max << 3;
    static const uint64_t dbg_network_max            = dbg_no_delay;

    enum class proto_type {
        unkown,
        tcp,
        ssl,
        udp,
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

        struct ext_t
        {
            proto_type proto_;
            std::string path_;

            ext_t() : proto_(proto_type::unkown) {}
            explicit ext_t(proto_type proto) : proto_(proto) {}
            ext_t(proto_type proto, std::string const& path) : proto_(proto), path_(path) {}
        };

        endpoint() {}
        endpoint(const endpoint&) = default;
        endpoint(endpoint&&) = default;
        endpoint& operator=(const endpoint&) = default;
        endpoint& operator=(endpoint&&) = default;

        using base_t::base_t;

        template <typename Proto>
            explicit endpoint(::boost::asio::ip::basic_endpoint<Proto> const& ep)
            : base_t(ep.address(), ep.port())
            {
            }

        template <typename Proto>
            explicit endpoint(::boost::asio::ip::basic_endpoint<Proto> const& ep,
                    proto_type proto)
            : base_t(ep.address(), ep.port()), ext_(proto)
            {
            }

        template <typename Proto>
            explicit endpoint(::boost::asio::ip::basic_endpoint<Proto> const& ep,
                    ext_t const& ext)
            : base_t(ep.address(), ep.port()), ext_(ext)
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

        proto_type proto() const
        {
            return ext_.proto_;
        }

        void set_proto(proto_type proto)
        {
            ext_.proto_ = proto;
        }

        std::string const& path() const
        {
            return ext_.path_;
        }

        void set_path(std::string const& path)
        {
            ext_.path_ = path;
        }

        ext_t const& ext() const
        {
            return ext_;
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

    private:
        ext_t ext_;
    };

    typedef std::vector<char> Buffer;
    typedef boost::function<void(boost_ec const&)> SndCb;

    struct OptionsBase;
    struct SessionBase
    {
        virtual ~SessionBase() {}

        // functional
        virtual void SendNoDelay(Buffer && buf, SndCb const& cb = NULL) = 0;
        virtual void SendNoDelay(const void* data, size_t bytes, SndCb const& cb = NULL) = 0;
        virtual void Send(Buffer && buf, SndCb const& cb = NULL) = 0;
        virtual void Send(const void* data, size_t bytes, SndCb const& cb = NULL) = 0;
        virtual bool IsEstab() = 0;
        virtual void Shutdown(bool immediately = true) = 0;
        virtual boost_ec SetSocketOptNoDelay(bool is_nodelay) { return boost_ec(); }
        virtual endpoint LocalAddr() = 0;
        virtual endpoint RemoteAddr() = 0;

        // statistics
        virtual std::size_t GetSendQueueSize() = 0;

        // storage
        boost::any & Storage() { return storage_; }

    private:
        boost::any storage_;
    };

    struct FakeSession : public SessionBase
    {
        virtual void SendNoDelay(Buffer && buf, SndCb const& cb = NULL) override;
        virtual void SendNoDelay(const void* data, size_t bytes, SndCb const& cb = NULL) override;
        virtual void Send(Buffer && buf, SndCb const& cb = NULL) override;
        virtual void Send(const void* data, size_t bytes, SndCb const& cb = NULL) override;
        virtual bool IsEstab() override;
        virtual void Shutdown(bool immediately = false) override;
        virtual endpoint LocalAddr() override;
        virtual endpoint RemoteAddr() override;
        virtual std::size_t GetSendQueueSize() override;
    };

    class SessionEntry
    {
        static FakeSession fake_sess;
        typedef boost::shared_ptr<SessionBase> SessionImpl;
        SessionImpl impl_;

    public:
        SessionEntry() = default;
        SessionEntry(SessionImpl impl);

        template <typename S>
        SessionEntry(boost::shared_ptr<S> s_impl,
                typename std::enable_if<std::is_base_of<SessionBase, S>::value>::type* = nullptr)
        {
            impl_ = boost::static_pointer_cast<SessionBase>(s_impl);
        }

        // This method can avoid dereference null pointer crash.
        // It's always returns a valid pointer.
        inline SessionBase* operator->() const
        {
            return GetPtr();
        }

        friend bool operator<(SessionEntry const& lhs, SessionEntry const& rhs)
        {
            return lhs.GetPtr() < rhs.GetPtr();
        }
        friend bool operator==(SessionEntry const& lhs, SessionEntry const& rhs)
        {
            return lhs.GetPtr() < rhs.GetPtr();
        }

    private:
        inline SessionBase* GetPtr() const
        {
            return impl_ ? impl_.get() : (SessionBase*)&fake_sess;
        }
    };

    typedef boost::function<size_t(SessionEntry, const char* data, size_t bytes)> ReceiveCb;

    struct ServerBase
    {
        virtual ~ServerBase() {}
        virtual boost_ec goStart(endpoint addr) = 0;
        virtual void Shutdown(bool immediately = true) = 0;
        virtual OptionsBase* GetOptions() = 0;
        virtual boost_ec goStartBeforeFork(endpoint addr) = 0;
        virtual void goStartAfterFork() {}
        virtual endpoint LocalAddr() = 0;
    };
    struct ClientBase
    {
        virtual ~ClientBase() {}
        virtual boost_ec Connect(endpoint addr) = 0;
        virtual SessionEntry GetSession() = 0;
        virtual OptionsBase* GetOptions() = 0;
    };

    // ----- tcp protocol effect only ------
    typedef boost::function<void(SessionEntry)> ConnectedCb;
    typedef boost::function<void(SessionEntry, boost_ec const&)> DisconnectedCb;
    // -------------------------------------

    struct Protocol
    {
        typedef ::network::endpoint endpoint;
        virtual ~Protocol() {}

        static Protocol v4();
        static Protocol v6();
        int type() const;
        int protocol() const;
        int family() const;

        virtual boost::shared_ptr<ServerBase> CreateServer() { return boost::shared_ptr<ServerBase>(); };
        virtual boost::shared_ptr<ClientBase> CreateClient() { return boost::shared_ptr<ClientBase>(); };

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
