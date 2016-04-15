#include <libgonet/network.h>
#include <stdio.h>
#include <unistd.h>
#include <boost/thread.hpp>
#include <atomic>
#include <chrono>
#include <iostream>
using namespace network;
using namespace std;
using namespace std::chrono;

int blockSize = 1;
int timeout = 1;
std::atomic<int64_t> bytesRead_;
std::atomic<int64_t> bytesWritten_;
std::atomic<int64_t> messagesRead_;
time_point<system_clock> start_time;

void onConnection(SessionEntry sess)
{
    std::vector<char> message;
    for (int i = 0; i < blockSize; ++i)
        message.push_back(static_cast<char>(i % 128));

    sess->Send(std::move(message));
}

size_t onMessage(SessionEntry sess, const char* data, size_t bytes)
{
    ++messagesRead_;
    bytesRead_ += bytes;
    bytesWritten_ += bytes;
    sess->Send(data, bytes);
    return bytes;
}

void show()
{
    cout << bytesRead_ << " total bytes read" << endl;
    cout << messagesRead_ << " total messages read" << endl;
    cout << static_cast<double>(bytesRead_) / static_cast<double>(messagesRead_)
        << " average message size" << endl;
    cout << static_cast<double>(bytesRead_) / (timeout * 1024 * 1024)
        << " MiB/s throughput" << endl;
    cout << "elapse " << duration_cast<milliseconds>(system_clock::now() - start_time).count() 
        << " ms" << endl;
}

int main(int argc, char* argv[])
{
    if (argc != 7)
    {
        fprintf(stderr, "Usage: client <host_ip> <port> <threads> <blocksize> "
                "<sessions> <time>\n");
    }
    else
    {
        const char* ip = argv[1];
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        int threadCount = atoi(argv[3]);
        blockSize = atoi(argv[4]);
        int sessionCount = atoi(argv[5]);
        timeout = atoi(argv[6]);

        std::string url = std::string("tcp://") + ip +  ":" + std::to_string(port);
        start_time = system_clock::now();

        std::list<network::Client*> clients;
        for (int i = 0; i < sessionCount; ++i)
        {
            network::Client *client = new network::Client;
            client->SetConnectedCb(&onConnection);
            client->SetReceiveCb(&onMessage);
            boost_ec ec = client->Connect(url);
            if (ec) {
                printf("connect to %s:%d error: %s\n", ip, port, ec.message().c_str());
                return 1;
            }
            clients.push_back(client);
        }

        printf("connect server %s:%d\n", ip, port);

        go [&] {
            sleep(timeout);
            for (auto *c : clients)
                c->Shutdown();
            show();
            exit(0);
        };

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

