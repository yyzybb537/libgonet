#include "network.h"
#include "error.h"

namespace network {

    ProtocolRef::ProtocolRef(boost::shared_ptr<Protocol*> proto)
        : proto_(proto)
    {}

    Server::Server()
        : protocol_(new Protocol*(nullptr))
    {}
    boost_ec Server::goStart(std::string const& url)
    {
        boost_ec ec;
        local_addr_ = endpoint::from_string(url, ec);
        if (ec) return ec;

        if (local_addr_.proto_ == proto_type::tcp) {
            *protocol_ = tcp::instance();
        } else if (local_addr_.proto_ == proto_type::udp) {
            *protocol_ = udp::instance();
        } else {
            return MakeNetworkErrorCode(eNetworkErrorCode::ec_unsupport_protocol);
        }

        impl_ = (*protocol_)->CreateServer();
        this->Link(*impl_->GetOptions());
        return impl_->goStart(local_addr_);
    }
    void Server::Shutdown()
    {
        impl_.reset();
    }
    endpoint Server::LocalAddr()
    {
        return local_addr_;
    }
    ProtocolRef Server::GetProtocol()
    {
        return ProtocolRef(protocol_);
    }

    Client::Client()
        : protocol_(new Protocol*(nullptr)), connect_mtx_(new co_mutex)
    {}
    boost_ec Client::Connect(std::string const& url)
    {
        std::unique_lock<co_mutex> lock(*connect_mtx_, std::defer_lock);
        if (!lock.try_lock()) return MakeNetworkErrorCode(eNetworkErrorCode::ec_connecting);
        if (impl_ && protocol_ && (*protocol_)->IsEstab(impl_->GetSessId())) return MakeNetworkErrorCode(eNetworkErrorCode::ec_estab);

        boost_ec ec;
        local_addr_ = endpoint::from_string(url, ec);
        if (ec) return ec;

        if (local_addr_.proto_ == proto_type::tcp) {
            *protocol_ = tcp::instance();
        } else if (local_addr_.proto_ == proto_type::udp) {
            *protocol_ = udp::instance();
        } else {
            return MakeNetworkErrorCode(eNetworkErrorCode::ec_unsupport_protocol);
        }

        impl_ = (*protocol_)->CreateClient();
        this->Link(*impl_->GetOptions());
        return impl_->Connect(local_addr_);
    }
    void Client::Send(const void* data, size_t bytes, SndCb cb)
    {
        if (!protocol_) {
            if (cb)
                cb(MakeNetworkErrorCode(eNetworkErrorCode::ec_shutdown));
            return ;
        }

        (*protocol_)->Send(GetSessId(), data, bytes, cb);
    }
    void Client::Shutdown()
    {
        impl_.reset();
    }
    endpoint Client::LocalAddr()
    {
        return local_addr_;
    }
    endpoint Client::RemoteAddr()
    {
        if (*protocol_ && impl_)
            return (*protocol_)->RemoteAddr(impl_->GetSessId());

        return endpoint();
    }
    ProtocolRef Client::GetProtocol()
    {
        return ProtocolRef(protocol_);
    }
    SessionId Client::GetSessId()
    {
        return impl_ ? impl_->GetSessId() : SessionId();
    }

} //namespace network
