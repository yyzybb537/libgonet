#include <iostream>
#include <unistd.h>
#include <boost/thread.hpp>
#include <libgonet/network.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>
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

    int sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget returns -1. error:");
        return 0;
    }

    union {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
        struct seminfo  *__buf;
    } sem_v;
    sem_v.val = 1;
    if (-1 == semctl(sem_id, 0, SETVAL, sem_v)) {
        perror("semctl calls error:");
        return 0;
    }

    OptionsAcceptAspect aop_accept;
    aop_accept.before_aspect = [sem_id] {
        co_await(void) [sem_id]{
            sembuf sb;
            sb.sem_num = 0;
            sb.sem_op = -1;
            sb.sem_flg = 0;
            printf("[%d] call semop\n", getpid());
            semop(sem_id, &sb, 1);
        };
        printf("[%d] enter accept.\n", getpid());
    };
    aop_accept.after_aspect = [sem_id] {
        printf("[%d] exit accept.\n", getpid());
        sembuf sb;
        sb.sem_num = 0;
        sb.sem_op = 1;
        sb.sem_flg = 0;
        semop(sem_id, &sb, 1);
    };
    server.SetAcceptAspect(aop_accept);

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
        if (!fork())
            break;

    // thread must create after fork.
    boost::thread th([]{ co_sched.GetThreadPool().RunLoop(); });

    server.goStartAfterFork();

    co_sched.RunUntilNoTask();
}
