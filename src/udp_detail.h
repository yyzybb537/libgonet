#pragma once

#include <string>
#include <stdint.h>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <coroutine/coroutine.h>
#include "error.h"
#include "abstract.h"
#include "option.h"

namespace network {
namespace udp_detail {

using namespace boost::asio;
using namespace boost::asio::ip;
using boost_ec = boost::system::error_code;
using boost::shared_ptr;

class UdpPointImpl;
struct _udp_sess_id_t : public ::network::SessionIdBase
{
    shared_ptr<UdpPointImpl> udp_point;
    udp::endpoint remote_addr;

    _udp_sess_id_t(shared_ptr<UdpPointImpl> const& point,
            udp::endpoint const& addr)
        : udp_point(point), remote_addr(addr)
        {}
};
typedef shared_ptr<_udp_sess_id_t> udp_sess_id_t;

class UdpPointImpl
    : public Options<UdpPointImpl>, public boost::enable_shared_from_this<UdpPointImpl>
{
public:
    UdpPointImpl();

    boost_ec goStart(endpoint addr);
    void Shutdown();
    boost_ec Send(std::string const& host, uint16_t port, const void* data, std::size_t bytes);
    boost_ec Send(udp::endpoint destition, const void* data, std::size_t bytes);
    boost_ec Connect(endpoint addr);
    boost_ec Send(const void* data, size_t bytes);
    udp::endpoint LocalAddr();
    udp::endpoint RemoteAddr();
    udp_sess_id_t GetSessId();

private:
    virtual void OnSetMaxPackSize() override;
    boost_ec goStart(udp::endpoint local_endpoint);
    void DoRecv();

    static io_service& GetUdpIoService();

private:
    udp::endpoint local_addr_;
    udp::endpoint remote_addr_;
    shared_ptr<udp::socket> socket_;
    std::atomic<bool> shutdown_{false};
    std::atomic<bool> init_{false};
    Buffer recv_buf_;
};

class UdpPoint
    : public Options<UdpPoint>, public ServerBase, public ClientBase
{
public:
    UdpPoint() : impl_(new UdpPointImpl())
    {
        Link(*impl_);
    }

    virtual ~UdpPoint()
    {
        Shutdown();
    }

    boost_ec goStart(endpoint addr)
    {
        return impl_->goStart(addr);
    }
    void Shutdown()
    {
        return impl_->Shutdown();
    }
    boost_ec Send(std::string const& host, uint16_t port, const void* data, std::size_t bytes)
    {
        return impl_->Send(host, port, data, bytes);
    }
    boost_ec Send(udp::endpoint destition, const void* data, std::size_t bytes)
    {
        return impl_->Send(destition, data, bytes);
    }
    boost_ec Connect(endpoint addr)
    {
        return impl_->Connect(addr);
    }
    boost_ec Send(const void* data, size_t bytes)
    {
        return impl_->Send(data, bytes);
    }
    udp::endpoint LocalAddr()
    {
        return impl_->LocalAddr();
    }
    udp::endpoint RemoteAddr()
    {
        return impl_->RemoteAddr();
    }
    OptionsBase* GetOptions()
    {
        return this;
    }
    SessionId GetSessId()
    {
        return impl_->GetSessId();
    }

private:
    shared_ptr<UdpPointImpl> impl_;
};

typedef UdpPoint UdpServer;
typedef UdpPoint UdpClient;

} //namespace udp_detail
} //namespace network


