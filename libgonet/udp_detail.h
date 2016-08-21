#pragma once

#include "config.h"
#include "error.h"
#include "abstract.h"
#include "option.h"

namespace network {
namespace udp_detail {

using namespace boost::asio;
using namespace boost::asio::ip;
using boost_ec = boost::system::error_code;
using boost::shared_ptr;

class UdpPoint;
struct _udp_sess_id_t : public ::network::SessionBase
{
    shared_ptr<UdpPoint> udp_point;
    endpoint remote_addr;

    _udp_sess_id_t(shared_ptr<UdpPoint> const& point,
            endpoint const& addr)
        : udp_point(point), remote_addr(addr)
    {}

    virtual void SendNoDelay(Buffer && buf, SndCb const& cb = NULL) override;
    virtual void SendNoDelay(const void* data, size_t bytes, SndCb const& cb = NULL) override;
    virtual void Send(Buffer && buf, SndCb const& cb = NULL) override;
    virtual void Send(const void* data, size_t bytes, SndCb const& cb = NULL) override;
    virtual bool IsEstab() override;
    virtual void Shutdown(bool immediately = true) override;
    virtual endpoint LocalAddr() override;
    virtual endpoint RemoteAddr() override;
    virtual std::size_t GetSendQueueSize() override;
};
typedef boost::shared_ptr<_udp_sess_id_t> udp_sess_id_t;

class UdpPoint
    : public Options<UdpPoint>, public ServerBase, public ClientBase,
    public boost::enable_shared_from_this<UdpPoint>
{
public:
    UdpPoint();

    boost_ec goStartBeforeFork(endpoint addr) override;
    boost_ec goStart(endpoint addr) override;
    void Shutdown(bool immediately = true) override;
    boost_ec Connect(endpoint addr) override;
    boost_ec Send(std::string const& host, uint16_t port, const void* data, std::size_t bytes);
    boost_ec Send(endpoint destition, const void* data, std::size_t bytes);
    boost_ec Send(const void* data, size_t bytes);
    endpoint LocalAddr() override;
    endpoint RemoteAddr();
    OptionsBase* GetOptions() override { return this; }
    SessionEntry GetSession() override;

private:
    virtual void OnSetMaxPackSize() override;
    void DoRecv();

    static io_service& GetUdpIoService();

private:
    endpoint local_addr_;
    endpoint remote_addr_;
    shared_ptr<udp::socket> socket_;
    co::atomic_t<bool> shutdown_{false};
    co::atomic_t<bool> init_{false};
    Buffer recv_buf_;
    co::co_chan<void> recv_shutdown_channel_{1};
};


typedef UdpPoint UdpServer;
typedef UdpPoint UdpClient;

} //namespace udp_detail
} //namespace network


