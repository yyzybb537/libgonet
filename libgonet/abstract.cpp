#include "abstract.h"

namespace network {

    void FakeSession::SendNoDelay(Buffer &&, const SndCb & cb)
    {
        if (cb) cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
    }
    void FakeSession::SendNoDelay(const void *, size_t, const SndCb & cb)
    {
        if (cb) cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
    }
    void FakeSession::Send(Buffer &&, const SndCb & cb)
    {
        if (cb) cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
    }
    void FakeSession::Send(const void *, size_t, const SndCb & cb)
    {
        if (cb) cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
    }
    bool FakeSession::IsEstab()
    {
        return false;
    }
    void FakeSession::Shutdown(bool)
    {
    }
    network::endpoint FakeSession::LocalAddr()
    {
        return network::endpoint();
    }
    network::endpoint FakeSession::RemoteAddr()
    {
        return network::endpoint();
    }
    std::size_t FakeSession::GetSendQueueSize()
    {
        return 0;
    }

    SessionEntry::SessionEntry(SessionImpl impl)
        : impl_(impl)
    {}

    network::SessionBase* SessionEntry::operator->() const 
    {
        return GetPtr();
    }

    SessionBase* SessionEntry::GetPtr() const
    {
        if (!impl_) {
            static FakeSession fake_sess;
            return &fake_sess;
        }

        return impl_.get();
    }

    char const* proto_type_s[] = {
        "unkown",
        "tcp",
        "ssl",
        "udp",
        "http",
        "https",
        "zk",
    };

    proto_type str2proto(std::string const& s)
    {
        static int n = sizeof(proto_type_s) / sizeof(char const*);
        for (int i = 0; i < n; ++i)
            if (strcmp(s.c_str(), proto_type_s[i]) == 0)
                return proto_type(i);

        return proto_type::unkown;
    }
    std::string proto2str(proto_type proto)
    {
        static int n = sizeof(proto_type_s) / sizeof(char const*);
        if ((int)proto >= n)
            return proto_type_s[0];

        return proto_type_s[(int)proto];
    }

    Protocol Protocol::v4()
    {
        return Protocol(0);
    }
    Protocol Protocol::v6()
    {
        return Protocol(0);
    }
    int Protocol::type() const
    {
        switch (proto_)
        {
            case proto_type::unkown:
                break;
            case proto_type::tcp:
            case proto_type::ssl:
                return ::boost::asio::ip::tcp::v4().type();
            case proto_type::udp:
                return ::boost::asio::ip::udp::v4().type();
            case proto_type::http:
                return ::boost::asio::ip::tcp::v4().type();
            case proto_type::https:
                break;
            case proto_type::zk:
                break;
        }
        return 0;
    }
    int Protocol::protocol() const
    {
        switch (proto_)
        {
            case proto_type::unkown:
                break;
            case proto_type::tcp:
            case proto_type::ssl:
                return ::boost::asio::ip::tcp::v4().protocol();
            case proto_type::udp:
                return ::boost::asio::ip::udp::v4().protocol();
            case proto_type::http:
                return ::boost::asio::ip::tcp::v4().protocol();
            case proto_type::https:
                break;
            case proto_type::zk:
                break;
        }
        return 0;
    }
    int Protocol::family() const
    {
        return family_;
    }

    std::string Protocol::endpoint::to_string(boost_ec & ec) const
    {
        std::string url;
        if (proto_ != proto_type::unkown) {
            url += proto2str(proto_) + "://";
        }
        url += address().to_string(ec);
        if (ec) return "";

        url += ":";
        url += std::to_string(port());
        url += path_;
        return url;
    }

    endpoint Protocol::endpoint::from_string(std::string const& url, boost_ec & ec)
    {
        if (url.empty()) {
            ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_url_parse_error);
            return endpoint();
        }

        static ::boost::regex re("((.*)://)?([^:/]+)(:(\\d+))?(/.*)?");                
        boost::smatch result;                                                          
        bool ok = boost::regex_match(url, result, re);
        if (!ok) {
            ec = MakeNetworkErrorCode(eNetworkErrorCode::ec_url_parse_error);
            return endpoint();
        }

        endpoint ep(::boost::asio::ip::address::from_string(result[3].str(), ec), atoi(result[5].str().c_str()));
        if (ec) return endpoint();

        ep.proto_ = str2proto(result[2].str());
        ep.path_ = result[6].str();
        return ep;
    }

} //namespace network
