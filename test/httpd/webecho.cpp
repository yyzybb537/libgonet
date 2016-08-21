#include <iostream>
#include <unistd.h>
#include <libgonet/network.h>
#if PROFILE
#include <gperftools/profiler.h>
#endif
using namespace std;
using namespace co;
using namespace network;

int main(int argc, char** argv)
{
    std::string url = "tcp://127.0.0.1:9001";
    if (argc > 1) {
        url = argv[1];
    }

    std::string html = "HTTP/1.1 200 OK\r\n\
Date: Fri, 19 Aug 2016 16:25:26 GMT\r\n\
Server: libgonet\r\n\
Content-Type: text/html\r\n\
Content-Length: 12\r\n\
Connection: Keep-Alive\r\n\r\n\
hello world!";

    Server server;

    int connection = 0;

    server.SetConnectedCb([&](SessionEntry sess){
        ++ connection;
#if PROFILE
        ProfilerStart("webecho.prof");
#endif
        printf("connected from %s:%d conn=%d\n", sess->RemoteAddr().address().to_string().c_str(), sess->RemoteAddr().port(), connection);
    }).SetDisconnectedCb([&](SessionEntry, boost_ec const& ec) {
        -- connection;
        printf("disconnected. reason %d:%s conn=%d\n", ec.value(), ec.message().c_str(), connection);
#if PROFILE
        if (!connection)
            ProfilerStop();
#endif
    }).SetReceiveCb([&](SessionEntry sess, const char* data, size_t bytes){
        // find http header split chars
//        static const char* splits = "\r\n\r\n";
//        for (int i = (int)bytes - 4; i >= 0; --i) {
//            if (*(int*)(data + i) == *(const int*)(const char*)(&splits[0])) {
                // http header
                sess->SendNoDelay(html.c_str(), html.length());
                return bytes;
//            }
//        }
//
//        return bytes - 3;
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
