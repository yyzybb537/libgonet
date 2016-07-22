#include <iostream>
#include <unistd.h>
#include <string.h>
#include <libgonet/network.h>
using namespace std;
using namespace co;
using namespace network;

void on_disconnect(SessionEntry sess, boost_ec const& ec)
{
    printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
}

void foo(std::string url)
{
    co_sched.GetOptions().debug = dbg_session_alive;
    Client client;

#if ENABLE_SSL
    OptionSSL ssl_opt;
    ssl_opt.certificate_chain_file = "server.crt";
    ssl_opt.private_key_file = "server.key";
    ssl_opt.tmp_dh_file = "dh2048.pem";
    client.SetSSLOption(ssl_opt);
#endif

    client.SetConnectedCb([&](SessionEntry sess){
        printf("connected.\n");
    })
    .SetDisconnectedCb(&on_disconnect)
    .SetReceiveCb([](SessionEntry sess, const char* data, size_t bytes){
            printf("receive: %.*s\n", (int)bytes, data);
            return bytes;
        });

    boost_ec ec = client.Connect(url);
    if (ec) {
        printf("connect error %d:%s\n", ec.value(), ec.message().c_str());
    } else {
        printf("connect to %s:%d\n", client.RemoteAddr().address().to_string().c_str(),
                client.RemoteAddr().port());
    }

    go [&] {
        int i = 1;
        for (;;++i)
        {
            if (client.IsEstab()) {
                if (i % 3 == 0)
                    client.Send("shutdown", 8, [](boost_ec ec){
                        printf("send returns %s\n", ec.message().c_str());
                        });
                else
                    client.Send("ping", 4, [](boost_ec ec){
                        printf("send returns %s\n", ec.message().c_str());
                        });
            }
            else
                printf("not estab.\n");

            ::sleep(3);
        }
    };

    for (;;)
    {
        if (client.IsEstab())
            co_yield;
        else
            client.Connect(url);
    }
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s url\n\n", argv[0]);
        exit(1);
    }

    std::string url = "tcp://127.0.0.1:3030";
    if (argc > 1) {
        url = argv[1];
    }

    go [url]{ foo(url); };
    co_sched.RunUntilNoTask();
    return 0;
}

