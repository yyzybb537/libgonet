#include "network.h"
#include "error.h"

namespace network {

    Server::Server()
        : local_addr_(new endpoint)
    {}
    boost_ec Server::goStart(std::string const& url)
    {
        boost_ec ec;
        *local_addr_ = endpoint::from_string(url, ec);
        if (ec) return ec;

        if (local_addr_->proto_ == proto_type::tcp) {
            protocol_ = tcp::instance();
        } else if (local_addr_->proto_ == proto_type::udp) {
            protocol_ = udp::instance();
        } else {
            return MakeNetworkErrorCode(eNetworkErrorCode::ec_unsupport_protocol);
        }

        impl_ = protocol_->CreateServer();
        this->Link(*impl_->GetOptions());
        return impl_->goStart(*local_addr_);
    }
    void Server::Shutdown()
    {
        impl_.reset();
    }
    endpoint Server::LocalAddr()
    {
        return *local_addr_;
    }

    Protocol const& Server::GetProtocol()
    {
        return *protocol_;
    }

    Client::Client()
        : connect_mtx_(new co_mutex), local_addr_(new endpoint)
    {}
    boost_ec Client::Connect(std::string const& url)
    {
        std::unique_lock<co_mutex> lock(*connect_mtx_, std::defer_lock);
        if (!lock.try_lock()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_connecting);
        if (impl_ && impl_->GetSession()->IsEstab()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_estab);

        boost_ec ec;
        *local_addr_ = endpoint::from_string(url, ec);
        if (ec) return ec;

        if (local_addr_->proto_ == proto_type::tcp) {
            protocol_ = tcp::instance();
        } else if (local_addr_->proto_ == proto_type::udp) {
            protocol_ = udp::instance();
        } else {
            return MakeNetworkErrorCode(eNetworkErrorCode::ec_unsupport_protocol);
        }

        impl_ = protocol_->CreateClient();
        this->Link(*impl_->GetOptions());
        return impl_->Connect(*local_addr_);
    }
    void Client::Send(Buffer && buf, SndCb const& cb)
    {
        if (!impl_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        impl_->GetSession()->Send(std::move(buf), cb);
    }
    void Client::Send(const void* data, size_t bytes, SndCb const& cb)
    {
        if (!impl_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        impl_->GetSession()->Send(data, bytes, cb);
    }
    bool Client::IsEstab()
    {
        return impl_ && impl_->GetSession()->IsEstab();
    }
    void Client::Shutdown()
    {
        impl_.reset();
    }
    endpoint Client::LocalAddr()
    {
        return *local_addr_;
    }
    endpoint Client::RemoteAddr()
    {
        return impl_ ? impl_->GetSession()->RemoteAddr() : endpoint();
    }
    SessionEntry Client::GetSession()
    {
        return impl_ ? impl_->GetSession() : SessionEntry();
    }
    Protocol const& Client::GetProtocol()
    {
        return *protocol_;
    }

} //namespace network
