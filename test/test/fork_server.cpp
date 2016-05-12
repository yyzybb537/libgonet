#include <iostream>
#include <unistd.h>
#include <libgonet/network.h>
using namespace std;
using namespace co;
using namespace network;

int main(int argc, char** argv)
{
    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s [child_count] [url]\n\n", argv[0]);
        exit(1);
    }

    int childs = atoi(argv[1]);

    std::string url = "tcp://0.0.0.0:3030";
    if (argc > 2) {
        url = argv[2];
    }

    Server server;

#if ENABLE_SSL
    OptionSSL ssl_opt;
    ssl_opt.certificate_chain_file = "server.crt";
    ssl_opt.private_key_file = "server.key";
    ssl_opt.tmp_dh_file = "dh2048.pem";
    server.SetSSLOption(ssl_opt);
#endif

    server.SetConnectedCb([&](SessionEntry sess){
        printf("[%d] connected from %s:%d\n", getpid(), sess->RemoteAddr().address().to_string().c_str(), sess->RemoteAddr().port());
    }).SetDisconnectedCb([](SessionEntry, boost_ec const& ec) {
        printf("[%d] disconnected. reason %d:%s\n", getpid(), ec.value(), ec.message().c_str());
    }).SetReceiveCb([&](SessionEntry sess, const char* data, size_t bytes){
        printf("[%d] receive: %.*s\n", getpid(), (int)bytes, data);
        sess->Send(data, bytes);
        if (strstr(std::string(data, bytes).c_str(), "shutdown")) {
            sess->Shutdown();
        }
        return bytes;
    });
    boost_ec ec = server.goStartBeforeFork(url);
    if (ec) {
        printf("server start error %d:%s\n", ec.value(), ec.message().c_str());
    } else {
        printf("server start at %s:%d\n", server.LocalAddr().address().to_string().c_str(),
                server.LocalAddr().port());
    }

    for (int i = 0; i < childs; ++i)
        fork();

    server.goStartAfterFork();

    co_sched.RunUntilNoTask();
}
