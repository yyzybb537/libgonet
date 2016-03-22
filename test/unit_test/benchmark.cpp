#include <iostream>
#include <unistd.h>
#include <gtest/gtest.h>
#include <libgo/coroutine.h>
#include <boost/thread.hpp>
#include <atomic>
#include <libgonet/network.h>
using namespace std;
using namespace co;
using namespace network;

#define MB / (1024 * 1024)

std::string g_url = "tcp://127.0.0.1:3050";
int g_thread_count = 2;
std::atomic<int> g_conn{0};
std::atomic<unsigned long long> g_server_send{0};
std::atomic<unsigned long long> g_server_send_err{0};
std::atomic<unsigned long long> g_server_recv{0};
std::atomic<unsigned long long> g_client_send{0};
std::atomic<unsigned long long> g_client_send_err{0};
std::atomic<unsigned long long> g_client_recv{0};

int recv_buffer_length = 40960;
char g_data[16 * 1024] = {1, 2};
const size_t g_max_pack = sizeof(g_data);
int pipeline = 10000;

void start_server(std::string url, bool *bexit)
{
    Server s;
    s.SetMaxPackSize(recv_buffer_length);
    s.SetDisconnectedCb([&](SessionEntry, boost_ec const&){
//                printf("server: disconnected!\n");
                })
        .SetReceiveCb(
                [&](SessionEntry sess, const void* data, size_t bytes)
                {
//                    printf("recv %u bytes from %s:%d\n", (uint32_t)bytes,
//                        s.RemoteAddr(sess).address().to_string().c_str(),
//                        s.RemoteAddr(sess).port());

                    g_server_recv += bytes;
                    if (!bytes)
                        printf("error bytes is zero.\n");

//                    if ((int)bytes > g_max_pack)
//                        g_max_pack = bytes;

                    size_t send_bytes = 0;
                    for (;send_bytes < bytes; send_bytes += g_max_pack) {
                        size_t send_b = std::min(bytes - send_bytes, g_max_pack);
                        sess->Send(data, send_b, [&, send_b](boost_ec ec){
                                if (ec) g_server_send_err += send_b;
                                else g_server_send += send_b;
                            });
                    }
                    return bytes;
                });
    boost_ec ec = s.goStart(url);
    ASSERT_FALSE(!!ec);

    while (!*bexit)
        sleep(1);

    s.Shutdown();
//    printf("server exit\n");
}

void start_client(std::string url, bool *bexit)
{
    Client c;
    c.SetMaxPackSize(recv_buffer_length);
    c.SetConnectedCb([&](SessionEntry){ ++g_conn; })
        .SetReceiveCb(
            [&](SessionEntry sess, const void* data, size_t bytes)
            {
                g_client_recv += bytes;

                size_t send_bytes = 0;
                for (;send_bytes < bytes; send_bytes += g_max_pack)
                {
                    size_t send_b = std::min(bytes - send_bytes, g_max_pack);
                    sess->Send(data, send_b, [&, send_b](boost_ec ec){
                        if (ec) g_client_send_err += send_b;
                        else g_client_send += send_b;
                        });
                }

                return bytes;
            });
    c.SetDisconnectedCb([](SessionEntry, boost_ec ec){
//            printf("client: disconnected!\n");
            --g_conn;
            });
    boost_ec ec = c.Connect(url);
    if (ec) {
        if (!*bexit) {
            sleep(1);
            go [=]{ start_client(url, bexit); };
        }
        return ;
    }

    for (int i = 0; i < pipeline; ++i)
        c.Send(g_data, sizeof(g_data));

    while (!*bexit) {
        sleep(1);

        if (!c.IsEstab()) {
            go [=]{ start_client(url, bexit); };
            return ;
        }
    }

    c.Shutdown();
//    printf("client exit\n");
}

void show_status(bool* bexit)
{
    if (*bexit) {
        printf("show end\n");
        return ;
    }

    static int s_c = 0;
    if (s_c++ % 10 == 0) {
        // print title
        printf("--------------------------------------------------------------------------------------------------------\n");
        printf(" index |  conn  |   s_send   | s_send_err |   s_recv   |   c_send   | c_send_err |   c_recv   |   QPS   | max_pack\n");
    }

    static unsigned long long last_server_send{0};
    static unsigned long long last_server_send_err{0};
    static unsigned long long last_server_recv{0};
    static unsigned long long last_client_send{0};
    static unsigned long long last_client_send_err{0};
    static unsigned long long last_client_recv{0};

    unsigned long long server_send = g_server_send - last_server_send;
    unsigned long long server_send_err = g_server_send_err - last_server_send_err;
    unsigned long long server_recv = g_server_recv - last_server_recv;
    unsigned long long client_send = g_client_send - last_client_send;
    unsigned long long client_send_err = g_client_send_err - last_client_send_err;
    unsigned long long client_recv = g_client_recv - last_client_recv;

    printf("%6d | %6d | %7llu MB | %7llu MB | %7llu MB | %7llu MB | %7llu MB | %7llu MB |%8llu | %d\n",
            s_c, (int)g_conn,
            server_send MB, server_send_err MB, server_recv MB,
            client_send MB, client_send_err MB, client_recv MB,
            (client_recv / std::max((int)g_max_pack, 1)), (int)g_max_pack);

    last_server_send = g_server_send;
    last_server_send_err = g_server_send_err;
    last_server_recv = g_server_recv;
    last_client_send = g_client_send;
    last_client_send_err = g_client_send_err;
    last_client_recv = g_client_recv;

    co_timer_add(std::chrono::seconds(1), [=]{ show_status(bexit); });
}

using ::testing::TestWithParam;
using ::testing::Values;

struct Benchmark : public TestWithParam<int>
{
    int n_;
    void SetUp() { n_ = GetParam(); }
};

TEST_P(Benchmark, BenchmarkT)
{
//    co_sched.GetOptions().debug = network::dbg_session_alive;
//    co_sched.GetOptions().debug = network::dbg_session_alive | co::dbg_hook;

    bool *bexit = new bool(false);
//    bool bexit = true;
    go [&]{ start_server(g_url, bexit); };
    
    for (int i = 0; i < n_; ++i)
        go [&]{ start_client(g_url, bexit); };

    co_timer_add(std::chrono::seconds(10), [=]{ *bexit = true; });
    co_timer_add(std::chrono::milliseconds(500), [=]{ show_status(bexit); });

//    co_sched.RunUntilNoTask();

    boost::thread_group tg;
    for (int i = 0; i < g_thread_count; ++i)
        tg.create_thread([]{ co_sched.RunUntilNoTask(); });
    tg.join_all();
}


INSTANTIATE_TEST_CASE_P(
        BenchmarkTest,
        Benchmark,
        Values(1, 10));
//Values(100, 1000, 10000));
