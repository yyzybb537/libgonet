#include <iostream>
#include <unistd.h>
#include <boost/thread.hpp>
#include <atomic>
#include <libgonet/network.h>
#if PROFILE
#include <gperftools/profiler.h>
#endif
using namespace std;
using namespace co;
using namespace network;

#define MB / (1024 * 1024)

std::string g_url = "tcp://127.0.0.1:3050";
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
bool g_nodelay_flag = false;
int g_thread_count = 1;

void start_client(std::string url)
{
    Client c;

#if ENABLE_SSL
    OptionSSL ssl_opt;
    ssl_opt.certificate_chain_file = "server.crt";
    ssl_opt.private_key_file = "server.key";
    ssl_opt.tmp_dh_file = "dh2048.pem";
    c.SetSSLOption(ssl_opt);
#endif

    c.SetMaxPackSize(recv_buffer_length);
    c.SetMaxPackSizeHard(-1);
    c.SetConnectedCb([&](SessionEntry sess){
            ++g_conn;
            sess->SetSocketOptNoDelay(g_nodelay_flag);
#if PROFILE
            ProfilerStart("client.prof");
#endif
            })
        .SetReceiveCb(
            [&](SessionEntry sess, const void* data, size_t bytes)
            {
                g_client_recv += bytes;
                ++g_qps;

                size_t pos = 0;
                while (pos < bytes) {
                    size_t n = std::min<size_t>(g_package, bytes - pos);
                    if (g_nodelay_flag) {
                        sess->SendNoDelay((char*)data + pos, n
//                            , [&, n](boost_ec ec){
//                                if (ec) g_client_send_err += n;
//                                else g_client_send += n;
//                            }
                            );
                    } else {
                        sess->Send((char*)data + pos, n
//                            , [&, n](boost_ec ec){
//                                if (ec) g_client_send_err += n;
//                                else g_client_send += n;
//                            }
                            );
                    }
                    pos += n;
                }

                return bytes;
            });
    c.SetDisconnectedCb([](SessionEntry, boost_ec ec){
//            printf("client: disconnected!\n");
            --g_conn;
#if PROFILE
            ProfilerStop();
#endif
//            cout << "disconnect error:" << ec.message() << endl;
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
        printf("------ start PackageSize=%d Bytes, Conn=%d, Pipeline=%d, NoDelay=%d, RecvBuffer=%d KB, Threads=%d URL=%s -----\n",
                g_package, conn, pipeline, g_nodelay_flag, recv_buffer_length / 1024, g_thread_count, g_url.c_str());
        printf(" index |  conn  |   s_send   | s_send_err |   s_recv   |   c_send   | c_send_err |   c_recv   |   OPS   | max_pack | Qps\n");
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

int main(int argc, char** argv)
{
//    co_sched.GetOptions().debug = network::dbg_no_delay | network::dbg_session_alive;
//    co_sched.GetOptions().debug_output = fopen("logclient", "w");
//    co_sched.GetOptions().enable_coro_stat = true;
//    co_sched.GetOptions().debug = network::dbg_session_alive | co::dbg_hook;
    co_sched.GetOptions().enable_work_steal = false;

    if (argc > 1 && argv[1] == std::string("-h")) {
        printf("Usage %s [PackageSize] [Conn] [Pipeline] [NoDelay] [recv_buffer_length(KB)] [Threads] [URL]\n\n", argv[0]);
        printf("Defaults [PackageSize=%d] [Conn=%d] [Pipeline=%d] [NoDelay=%d] [recv_buffer_length=%d(KB)] [Threads=%d] [URL=%s]\n\n",
                g_package, conn, pipeline, g_nodelay_flag, recv_buffer_length / 1024, g_thread_count, g_url.c_str());
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
        g_nodelay_flag = !!atoi(argv[4]);

    if (argc > 5)
        recv_buffer_length = atoi(argv[5]) * 1024;

    if (argc > 6)
        g_thread_count = atoi(argv[6]);

    if (argc > 7)
        g_url = argv[7];

    for (int i = 0; i < conn; ++i)
        go [&]{ start_client(g_url); };

    co_timer_add(std::chrono::milliseconds(100), [=]{ show_status(); });
    boost::thread_group tg;
    for (int i = 0; i < g_thread_count; ++i)
        tg.create_thread([]{ co_sched.RunLoop(); });
    tg.join_all();
    return 0;
}
