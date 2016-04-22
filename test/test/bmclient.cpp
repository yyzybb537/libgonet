#include <iostream>
#include <unistd.h>
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
std::atomic<unsigned long long> g_qps{0};

const int test_seconds = 4;
int recv_buffer_length = 160 * 1024;
int g_package = 64;
char *g_data = new char[g_package];
int pipeline = 1000;
int conn = 1;

void start_client(std::string url)
{
    Client c;
    c.SetMaxPackSize(recv_buffer_length);
    c.SetConnectedCb([&](SessionEntry){ ++g_conn; })
        .SetReceiveCb(
            [&](SessionEntry sess, const void* data, size_t bytes)
            {
                g_client_recv += bytes;
                ++g_qps;

                size_t pos = 0;
                while (pos < bytes) {
                    size_t n = std::min<size_t>(g_package, bytes - pos);
                    sess->Send((char*)data + pos, n, [&, n](boost_ec ec){
                        if (ec) g_client_send_err += n;
                        else g_client_send += n;
                        });
                    pos += n;
                }

                return bytes;
            });
    c.SetDisconnectedCb([](SessionEntry, boost_ec ec){
//            printf("client: disconnected!\n");
            --g_conn;
            });
    boost_ec ec = c.Connect(url);
    if (ec) {
        sleep(1);
        go [=]{ start_client(url); };
        return ;
    }

    for (int i = 0; i < pipeline; ++i)
        c.Send(g_data, g_package);

//    printf("send ok\n");

    for (;;)
        co_sleep(10000);
}

void show_status()
{
    static int s_c = 0;
    if (s_c++ % 10 == 0) {
        // print title
        printf("--------------------------------------------------------------------------------------------------------\n");
        printf(" index |  conn  |   s_send   | s_send_err |   s_recv   |   c_send   | c_send_err |   c_recv   |   QPS   | max_pack | Ops\n");
    }

    static unsigned long long last_server_send{0};
    static unsigned long long last_server_send_err{0};
    static unsigned long long last_server_recv{0};
    static unsigned long long last_client_send{0};
    static unsigned long long last_client_send_err{0};
    static unsigned long long last_client_recv{0};
    static unsigned long long last_qps{0};

    unsigned long long server_send = g_server_send - last_server_send;
    unsigned long long server_send_err = g_server_send_err - last_server_send_err;
    unsigned long long server_recv = g_server_recv - last_server_recv;
    unsigned long long client_send = g_client_send - last_client_send;
    unsigned long long client_send_err = g_client_send_err - last_client_send_err;
    unsigned long long client_recv = g_client_recv - last_client_recv;
    unsigned long long qps = g_qps - last_qps;

    printf("%6d | %6d | %7llu MB | %7llu MB | %7llu MB | %7llu MB | %7llu MB | %7llu MB |%8llu | %8d | %lld\n",
            s_c, (int)g_conn,
            server_send MB, server_send_err MB, server_recv MB,
            client_send MB, client_send_err MB, client_recv MB,
            qps, (int)g_package, client_recv / g_package);

    last_server_send = g_server_send;
    last_server_send_err = g_server_send_err;
    last_server_recv = g_server_recv;
    last_client_send = g_client_send;
    last_client_send_err = g_client_send_err;
    last_client_recv = g_client_recv;
    last_qps = g_qps;

    co_timer_add(std::chrono::seconds(1), [=]{ show_status(); });
}

co_main(int argc, char** argv)
{
//    co_sched.GetOptions().debug = network::dbg_session_alive;
//    co_sched.GetOptions().enable_coro_stat = true;
//    co_sched.GetOptions().debug = network::dbg_session_alive | co::dbg_hook;

    if (argc > 1 && argv[1] == std::string("-h")) {
        printf("Usage %s [PackageSize] [Conn] [Pipeline] [recv_buffer_length(KB)]\n\n", argv[0]);
        return 1;
    }

    if (argc > 1) {
        g_package = atoi(argv[1]);
        delete g_data;
        g_data = new char[g_package];
    }

    if (argc > 2)
        conn = atoi(argv[2]);

    if (argc > 3)
        pipeline = atoi(argv[3]);

    if (argc > 4)
        recv_buffer_length = atoi(argv[4]) * 1024;

    printf("start PackageSize=%d Bytes, Conn=%d, Pipeline=%d, RecvBuffer=%d KB.\n",
            g_package, conn, pipeline, recv_buffer_length / 1024);


    for (int i = 0; i < conn; ++i)
        go [&]{ start_client(g_url); };

    co_timer_add(std::chrono::milliseconds(100), [=]{ show_status(); });
    return 0;
}
