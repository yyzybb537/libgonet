#include <libgonet/network.h>
#include <stdio.h>
#include <unistd.h>
#include <boost/thread.hpp>
#include <atomic>
using namespace network;

std::atomic<int> g_session_count{0};

void onConnection(SessionEntry sess)
{
    ++g_session_count;
    printf("%s:%d connected. sessions=%d\n",
            sess->RemoteAddr().address().to_string().c_str(),
            sess->RemoteAddr().port(),
            (int)g_session_count);
}

void onDisconnection(SessionEntry sess, boost_ec const& ec)
{
    --g_session_count;
    printf("%s:%d disconnected. sessions=%d\n",
            sess->RemoteAddr().address().to_string().c_str(),
            sess->RemoteAddr().port(),
            (int)g_session_count);
}

size_t onMessage(SessionEntry sess, const char* data, size_t bytes)
{
    sess->Send(data, bytes);
    return bytes;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: server <address> <port> <threads>\n");
    }
    else
    {
        const char* ip = argv[1];
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        int threadCount = atoi(argv[3]);
        std::string url = std::string("tcp://") + ip +  ":" + std::to_string(port);

        network::Server server;
        server.SetConnectedCb(&onConnection);
        server.SetDisconnectedCb(&onDisconnection);
        server.SetReceiveCb(&onMessage);
        boost_ec ec = server.goStart(url);
        if (ec) {
            printf("listen %s:%d error: %s\n", ip, port, ec.message().c_str());
            return 1;
        }

        printf("listen %s:%d\n", ip, port);

        boost::thread_group tg;
        for (int i = 0; i < threadCount; ++i)
        {
            tg.create_thread([]{
                    co_sched.RunLoop();
                    });
        }
        tg.join_all();
    }
}

