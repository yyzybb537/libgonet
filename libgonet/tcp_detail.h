#pragma once

#include "config.h"
#include "error.h"
#include "abstract.h"
#include "option.h"
#include "tcp_socket.h"

namespace network {
namespace tcp_detail {

using namespace boost::asio;
using namespace boost::asio::ip;
using boost_ec = boost::system::error_code;
using boost::shared_ptr;

class LifeHolder {};

io_service& GetTcpIoService();

class TcpServer;
class TcpSession
    : public Options<TcpSession>,
    public boost::enable_shared_from_this<TcpSession>,
    public SessionBase
{
public:
    struct Msg
    {
        struct shutdown_msg_t {};

        co::atomic_t<bool> timeout{false};
        bool send_half = false;
        bool shutdown = false;
        std::size_t pos = 0;
        uint64_t id;
        SndCb cb;
        co_timer_id tid;
        Buffer buf;

        Msg(uint64_t uid, SndCb ocb) : id(uid), cb(ocb) {}
        explicit Msg(shutdown_msg_t) : shutdown(true) {}
        void Done(boost_ec const& ec);
    };
    typedef co::co_chan<boost::shared_ptr<Msg>> MsgChan;
    typedef std::list<boost::shared_ptr<Msg>> MsgList;

    explicit TcpSession(shared_ptr<tcp_socket> s, shared_ptr<LifeHolder> holder,
            OptionsData & opt, endpoint::ext_t const& endpoint_ext);
    ~TcpSession();
    void goStart();
    SessionEntry GetSession();

    virtual void SendNoDelay(Buffer && buf, SndCb const& cb = NULL) override;
    virtual void SendNoDelay(const void* data, size_t bytes, SndCb const& cb = NULL) override;
    virtual void Send(Buffer && buf, SndCb const& cb = NULL) override;
    virtual void Send(const void* data, size_t bytes, SndCb const& cb = NULL) override;
    virtual void Shutdown(bool immediately = true) override;
    virtual boost_ec SetSocketOptNoDelay(bool is_nodelay) override;
    virtual bool IsEstab() override;
    virtual endpoint LocalAddr() override;
    virtual endpoint RemoteAddr() override;
    virtual std::size_t GetSendQueueSize() override;

private:
    void goReceive();
    void goSend();
    void SetCloseEc(boost_ec const& ec);
    void OnClose();
    void ShutdownSend();
    void ShutdownRecv();

private:
    shared_ptr<tcp_socket> socket_;
    shared_ptr<LifeHolder> holder_;
    Buffer recv_buf_;
    uint32_t max_pack_size_shrink_;
    uint32_t max_pack_size_hard_;
    uint64_t msg_id_;
    MsgChan msg_chan_;
    MsgList msg_send_list_;
    co::LFLock send_mtx_;
    bool sending_;
    co_mutex close_ec_mutex_;
    boost_ec close_ec_;

    co::atomic_t<bool> initiative_shutdown_{false};
    co::atomic_t<bool> send_shutdown_{false};
    co::atomic_t<bool> recv_shutdown_{false};
    co_mutex closed_;

    endpoint local_addr_;
    endpoint remote_addr_;
};

class TcpServer
    : public Options<TcpServer>, public ServerBase, public LifeHolder,
    public boost::enable_shared_from_this<TcpServer>
{
public:
    typedef std::map<::network::SessionEntry, shared_ptr<TcpSession>> Sessions;

    boost_ec goStartBeforeFork(endpoint addr) override;
    void goStartAfterFork() override;

    boost_ec goStart(endpoint addr) override;
    void Shutdown(bool immediately = true) override;
    endpoint LocalAddr() override;
    OptionsBase* GetOptions() override { return this; }

    std::size_t SessionCount();

private:
    void Accept();
    void OnSessionClose(::network::SessionEntry id, boost_ec const& ec);

private:
    shared_ptr<tcp::acceptor> acceptor_;
    endpoint local_addr_;
    co_mutex sessions_mutex_;
    Sessions sessions_;
    co::atomic_t<bool> shutdown_{false};
    friend TcpSession;
};


// Tcp客户端
// TcpSession与TcpClient互相智能指针, 断开连接时才会断开循环引用,
// 因此TcpClient析构时一定是连接已经断开, 无需等待.
class TcpClient
    : public Options<TcpClient>, public ClientBase, public LifeHolder, public boost::enable_shared_from_this<TcpClient>
{
public:
    boost_ec Connect(endpoint addr) override;

    SessionEntry GetSession() override;

    OptionsBase* GetOptions() override { return this; }

private:
    void OnSessionClose(::network::SessionEntry id, boost_ec const& ec);

private:
    shared_ptr<TcpSession> sess_;
    co_mutex connect_mtx_;
    friend TcpSession;
};

} //namespace tcp_detail
} //namespace network


