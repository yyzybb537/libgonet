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
int g_max_pack = 0;
bool g_nodelay_flag = false;
int g_thread_count = 1;

void start_server(std::string url)
{
    Server s;

#if ENABLE_SSL
    OptionSSL ssl_opt;
    ssl_opt.certificate_chain_file = "server.crt";
    ssl_opt.private_key_file = "server.key";
    ssl_opt.tmp_dh_file = "dh2048.pem";
    s.SetSSLOption(ssl_opt);
#endif

    s.SetListenBacklog(1024);
    s.SetMaxPackSize(recv_buffer_length);
    s.SetMaxPackSizeHard(-1);
    s.SetDisconnectedCb([&](SessionEntry, boost_ec const& ec){
#if PROFILE
                ProfilerStop();
#endif
                --g_conn;
//                cout << "disconnect error:" << ec.message() << endl;
                })
        .SetReceiveCb(
                [&](SessionEntry sess, const void* data, size_t bytes)
                {
//                    printf("recv %u bytes from %s:%d\n", (uint32_t)bytes,
//                        s.RemoteAddr(sess).address().to_string().c_str(),
//                        s.RemoteAddr(sess).port());

                    g_server_recv += bytes;
                    ++g_qps;
                    if (!bytes)
                        printf("error bytes is zero.\n");

                    if ((int)bytes > g_max_pack)
                        g_max_pack = bytes;

                    size_t pos = 0;
                    while (pos < bytes) {
                        size_t n = std::min<size_t>(g_package, bytes - pos);
                        if (g_nodelay_flag) {
                            sess->SendNoDelay((char*)data + pos, n
//                                , [&, n](boost_ec ec){
//                                    if (ec) g_server_send_err += n;
//                                    else g_server_send += n;
//                                }
                                );
                        } else {
                            sess->Send((char*)data + pos, n
//                                , [&, n](boost_ec ec){
//                                    if (ec) g_server_send_err += n;
//                                    else g_server_send += n;
//                                }
                                );
                        }
                        pos += n;
                    }

                    return bytes;
                });
    s.SetConnectedCb([&](SessionEntry sess) {
            ++g_conn;
            sess->SetSocketOptNoDelay(g_nodelay_flag);
#if PROFILE
            ProfilerStart("server.prof");
#endif
            });

    boost_ec ec = s.goStart(url);
    if (ec) {
        printf("server start error: %s\n", ec.message().c_str());
    }

    for (;;)
        co_sleep(10000);
}

void show_status()
{
    static int s_c = 0;
    if (s_c++ % 10 == 0) {
        // print title
        printf("--------------------------------------------------------------------------------------------------------\n");
        printf("------------- start PackageSize=%d Bytes, NoDelay=%d, RecvBuffer=%d KB, Threads=%d URL=%s -------------\n",
                g_package, g_nodelay_flag, recv_buffer_length / 1024, g_thread_count, g_url.c_str());
        printf(" index |  conn  |   s_send   | s_send_err |   s_recv   |   c_send   | c_send_err |   c_recv   |   OPS   | max_pack\n");
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

    printf("%6d | %6d | %7llu MB | %7llu MB | %7llu MB | %7llu MB | %7llu MB | %7llu MB |%8llu | %d\n",
            s_c, (int)g_conn,
            server_send MB, server_send_err MB, server_recv MB,
            client_send MB, client_send_err MB, client_recv MB,
            qps, (int)g_max_pack);

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
//    co_sched.GetOptions().debug_output = fopen("logserver", "w");
//    co_sched.GetOptions().enable_coro_stat = true;
//    co_sched.GetOptions().debug = network::dbg_session_alive | co::dbg_hook;
    co_sched.GetOptions().enable_work_steal = false;

    if (argc > 1 && argv[1] == std::string("-h")) {
        printf("Usage %s [PackageSize] [NoDelay] [recv_buffer_length(KB)] [Threads] [URL]\n\n", argv[0]);
        printf("Defaults [PackageSize=%d] [NoDelay=%d] [recv_buffer_length=%d(KB)] [Threads=%d] [URL=%s]\n\n",
                g_package, g_nodelay_flag, recv_buffer_length / 1024, g_thread_count, g_url.c_str());
        return 1;
    }

    if (argc > 1) {
        g_package = atoi(argv[1]);
    }

    if (argc > 2)
        g_nodelay_flag = !!atoi(argv[2]);

    if (argc > 3)
        recv_buffer_length = atoi(argv[3]) * 1024;

    if (argc > 4)
        g_thread_count = atoi(argv[4]);

    if (argc > 5)
        g_url = argv[5];

    go [&]{ start_server(g_url); };
    co_timer_add(std::chrono::milliseconds(100), [=]{ show_status(); });
    boost::thread_group tg;
    for (int i = 0; i < g_thread_count; ++i)
        tg.create_thread([]{ co_sched.RunLoop(); });
    tg.join_all();
    return 0;
}

