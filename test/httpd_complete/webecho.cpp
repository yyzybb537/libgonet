#include <iostream>
#include <unistd.h>
#include <libgonet/network.h>
#include <rapidhttp/document.h>
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

//HTTP/1.1 200 OK
//Date: Fri, 19 Aug 2016 16:25:26 GMT
//Server: libgonet
//Content-Type: text/html
//Content-Length: 12
//Connection: Keep-Alive
//
//hello world!";

    rapidhttp::HttpDocument response(rapidhttp::HttpDocument::Response);
    response.SetStatusCode(200);
    response.SetStatus("OK");
    response.SetField("Date", "Fri, 19 Aug 2016 16:25:26 GMT");
    response.SetField("Server", "libgonet");
    response.SetField("Content-Type", "text/html");
    response.SetField("Content-Length", "12");
    response.SetField("Connection", "Keep-Alive");
    response.SetBody("hello world!");

    std::string response_buf;
    response_buf.resize(response.ByteSize());

    Server server;

    int connection = 0;

    server.SetConnectedCb([&](SessionEntry sess){
        ++ connection;
#if PROFILE
        ProfilerStart("webecho.prof");
#endif
        printf("connected from %s:%d conn=%d\n", sess->RemoteAddr().address().to_string().c_str(), sess->RemoteAddr().port(), connection);
        sess->Storage() = boost::any(new rapidhttp::HttpDocument(rapidhttp::HttpDocument::Request));
    }).SetDisconnectedCb([&](SessionEntry, boost_ec const& ec) {
        -- connection;
        printf("disconnected. reason %d:%s conn=%d\n", ec.value(), ec.message().c_str(), connection);
#if PROFILE
        if (!connection)
            ProfilerStop();
#endif
    }).SetReceiveCb([&](SessionEntry sess, const char* data, size_t bytes){
        // find http header split chars
        rapidhttp::HttpDocument & doc = *boost::any_cast<rapidhttp::HttpDocument*>(sess->Storage());
        size_t nparsed = doc.PartailParse(data, bytes);
        if (doc.ParseError())
            return (size_t)-1;

        if (!doc.ParseDone())
            return nparsed;

        bool b = response.Serialize(&response_buf[0], response_buf.length());
        assert(b);
        sess->SendNoDelay(response_buf.c_str(), response_buf.length());
        return nparsed;
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
