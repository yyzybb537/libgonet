#include <iostream>
#include <unistd.h>
#include <libgonet/network.h>
using namespace std;
using namespace co;
using namespace network;

int main(int argc, char** argv)
{
    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s url\n\n", argv[0]);
        exit(1);
    }

    std::string url = "tcp://0.0.0.0:3030";
    if (argc > 1) {
        url = argv[1];
    }

    Server server;
    server.SetConnectedCb([&](SessionEntry sess){
        printf("connected from %s:%d\n", sess->RemoteAddr().address().to_string().c_str(), sess->RemoteAddr().port());
    }).SetDisconnectedCb([](SessionEntry, boost_ec const& ec) {
        printf("disconnected. reason %d:%s\n", ec.value(), ec.message().c_str());
    }).SetReceiveCb([&](SessionEntry sess, const char* data, size_t bytes){
        printf("receive: %.*s\n", (int)bytes, data);
        sess->Send(data, bytes);
        if (strstr(std::string(data, bytes).c_str(), "shutdown")) {
            sess->Shutdown();
        }
        return bytes;
    });
    boost_ec ec = server.goStart(url);
    if (ec) {
        printf("server start error %d:%s\n", ec.value(), ec.message().c_str());
    } else {
        printf("server start at %s:%d\n", server.LocalAddr().address().to_string().c_str(),
                server.LocalAddr().port());
    }

    co_sched.RunUntilNoTask();
}
