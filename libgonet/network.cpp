#include "network.h"
#include "error.h"

namespace network {

    Server::Server()
        : local_addr_(new endpoint)
    {
    }
    Server::~Server()
    {
        Shutdown(true);
    }
    boost_ec Server::goStartBeforeFork(std::string const& url)
    {
        boost_ec ec;
        *local_addr_ = endpoint::from_string(url, ec);
        if (ec) return ec;

        if (local_addr_->proto() == proto_type::tcp || local_addr_->proto() == proto_type::ssl) {
            protocol_ = tcp::instance();
        } else if (local_addr_->proto() == proto_type::udp) {
            protocol_ = udp::instance();
        } else {
            return MakeNetworkErrorCode(eNetworkErrorCode::ec_unsupport_protocol);
        }

        impl_ = protocol_->CreateServer();
        this->Link(*impl_->GetOptions());
        ec = impl_->goStartBeforeFork(*local_addr_);
        *local_addr_ = impl_->LocalAddr();
        return ec;
    }
    void Server::goStartAfterFork()
    {
        impl_->goStartAfterFork();
    }
    boost_ec Server::goStart(std::string const& url)
    {
        boost_ec ec;
        *local_addr_ = endpoint::from_string(url, ec);
        if (ec) return ec;

        if (local_addr_->proto() == proto_type::tcp || local_addr_->proto() == proto_type::ssl) {
            protocol_ = tcp::instance();
        } else if (local_addr_->proto() == proto_type::udp) {
            protocol_ = udp::instance();
        } else {
            return MakeNetworkErrorCode(eNetworkErrorCode::ec_unsupport_protocol);
        }

        impl_ = protocol_->CreateServer();
        this->Link(*impl_->GetOptions());
        ec = impl_->goStart(*local_addr_);
        *local_addr_ = impl_->LocalAddr();
        return ec;
    }
    void Server::Shutdown(bool immediately)
    {
        if (impl_)
            impl_->Shutdown(immediately);
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
        : connect_mtx_(new co_mutex), remote_addr_(new endpoint)
    {
    }
    Client::~Client()
    {
        Shutdown(true);
    }
    boost_ec Client::Connect(std::string const& url)
    {
        std::unique_lock<co_mutex> lock(*connect_mtx_, std::defer_lock);
        if (!lock.try_lock()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_connecting);
        if (impl_ && impl_->GetSession()->IsEstab()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_estab);

        boost_ec ec;
        *remote_addr_ = endpoint::from_string(url, ec);
        if (ec) return ec;

        if (remote_addr_->proto() == proto_type::tcp || remote_addr_->proto() == proto_type::ssl) {
            protocol_ = tcp::instance();
        } else if (remote_addr_->proto() == proto_type::udp) {
            protocol_ = udp::instance();
        } else {
            return MakeNetworkErrorCode(eNetworkErrorCode::ec_unsupport_protocol);
        }

        impl_ = protocol_->CreateClient();
        this->Link(*impl_->GetOptions());
        return impl_->Connect(*remote_addr_);
    }
    void Client::SendNoDelay(Buffer && buf, SndCb const& cb)
    {
        if (!impl_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        impl_->GetSession()->SendNoDelay(std::move(buf), cb);
    }
    void Client::SendNoDelay(const void* data, size_t bytes, SndCb const& cb)
    {
        if (!impl_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        impl_->GetSession()->SendNoDelay(data, bytes, cb);
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
    void Client::Shutdown(bool immediately)
    {
        if (impl_)
            impl_->GetSession()->Shutdown(immediately);
    }
    endpoint Client::LocalAddr()
    {
        return impl_ ? impl_->GetSession()->LocalAddr() : endpoint();
    }
    endpoint Client::RemoteAddr()
    {
        return *remote_addr_;
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
