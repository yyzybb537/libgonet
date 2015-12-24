#include <iostream>
#include <unistd.h>
#include <string.h>
#include <coroutine/coroutine.h>
#include "network.h"
using namespace std;
using namespace co;
using namespace network;

void on_disconnect(SessionId id, boost_ec const& ec)
{
    printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
}

void foo(std::string url)
{
    Client client;
    auto proto = client.GetProtocol();
    client.SetConnectedCb([proto](SessionId id){
        printf("connected.\n");
        go [id, proto] {
            for (;;)
            {
                if (proto->IsEstab(id))
                    proto->Send(id, "ping", 4, [](boost_ec ec){
                            printf("send returns %s\n", ec.message().c_str());
                        });
                else
                    return ;

                ::sleep(3);
            }
        };
    })
    .SetDisconnectedCb(&on_disconnect)
    .SetReceiveCb([](SessionId id, const char* data, size_t bytes){
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

    for (;;)
    {
        if (proto->IsEstab(client.GetSessId()))
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

