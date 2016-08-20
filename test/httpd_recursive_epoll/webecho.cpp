#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include <signal.h>
#include <libgo/coroutine.h>
#if PROFILE
#include <gperftools/profiler.h>
#endif
using namespace std;

#define CHECK_ERROR(res, syscall) \
    do { \
        if (res < 0) { \
            perror(#syscall " error:"); \
            return ; \
        } \
    } while(0)

static std::string html = "HTTP/1.1 200 OK\r\n"
    "Date: Fri, 19 Aug 2016 16:25:26 GMT\r\n"
    "Server: libgonet\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 12\r\n"
    "Connection: Keep-Alive\r\n\r\n"
    "hello world!";

int connection = 0;

inline void closefd(int fd)
{
    ::close(fd);
    if (--connection == 0) {
#if PROFILE
        ProfilerStop();
#endif
    }
    printf("conn: %d\n", connection);
}

inline bool set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    if (-1 == fcntl(fd, F_SETFL, flags)) {
        perror("fcntl error:");
        return false;
    }

    return true;
}

enum {
    shutdown_rd = 0x0,
    shutdown_wr = 0x1,
    shutdown_ep = 0x2,
    shutdown_all = 1 << shutdown_rd | 1 << shutdown_wr | 1 << shutdown_ep,
};

struct fdst
{
    int fd;
    int shutdown_;
    co_chan<bool> in{1024};
    co_chan<bool> out{1024};

    explicit fdst(int sfd)
        : fd(sfd), shutdown_(0) {}

    void shutdown()
    {
        ::shutdown(fd, SHUT_RDWR);
    }

    void close()
    {
        closefd(fd);
        delete this;
    }
};

void on_read(fdst *fds)
{
    for (;;) {
        bool estab = false;
        fds->in >> estab;
        if (!estab) {
            break;
        }

        int fd = fds->fd;

        // read
        static char buf[8192];
retry_read:
        ssize_t bytes = read(fd, buf, sizeof(buf));
        if (bytes < 0) {
            if (errno == EINTR)
                goto retry_read;
            else if (errno == EAGAIN)
                continue;
            else {
                fds->shutdown();
                break;
            }
        }

        if (bytes == 0) {
            // eof
            fds->shutdown();
            break;
        }

retry_write:
        bytes = write(fd, html.c_str(), html.length());
        if (bytes < 0) {
            if (errno == EINTR)
                goto retry_write;
            else if (errno == EAGAIN)
                goto retry_write;
            else {
                fds->shutdown();
                break;
            }
        }

        goto retry_read;
    }

    fds->shutdown_ |= shutdown_rd;
    if (fds->shutdown_ == shutdown_all)
        fds->close();
}

void on_write(fdst *fds)
{
    for (;;) {
        bool estab = false;
        fds->out >> estab;
        if (!estab) {
            break;
        }
    }

    fds->shutdown_ |= shutdown_wr;
    if (fds->shutdown_ == shutdown_all)
        fds->close();
}

void start()
{
    signal(SIGPIPE, SIG_IGN);

    int epfd = epoll_create(1024);
    CHECK_ERROR(epfd, epoll_create);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_ERROR(listenfd, socket);

    if (!set_nonblocking(listenfd)) {
        return ;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9001);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int res = bind(listenfd, (sockaddr*)&addr, sizeof(addr));
    CHECK_ERROR(res, bind);

    res = listen(listenfd, 1024);
    CHECK_ERROR(res, listen);

    epoll_event ev;
    ev.data.ptr = new fdst(listenfd);
    ev.events = EPOLLOUT | EPOLLIN | EPOLLET;
    res = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    CHECK_ERROR(res, epoll_ctl);

    for (;;) {
        static epoll_event evs[1024];

retry_epoll_wait:
        int n = libgo_epoll_wait(epfd, evs, 1024, 20);
        if (n < 0 && errno == EINTR)
            goto retry_epoll_wait;
        CHECK_ERROR(n, epoll_wait);

        for (int i = 0; i < n; ++i)
        {
            epoll_event & ev = evs[i];
            fdst *fds = (fdst *)ev.data.ptr;
            if (fds->fd == listenfd) {
                // listen fd
                if (ev.events & EPOLLIN) {
                    // accept
                    socklen_t len = sizeof(addr);
retry_accept:
                    int s = accept(listenfd, (sockaddr*)&addr, &len);
                    if (s < 0) {
                        if (errno == EINTR)
                            goto retry_accept;
                        else if (errno == EAGAIN)
                            continue;
                        else {
                            printf("listen error\n");
                            exit(1);
                        }
                    }

                    if (!set_nonblocking(s)) {
                        close(s);
                        goto retry_accept;
                    }

                    fdst* fds = new fdst(s);
                    epoll_event socket_ev;
                    socket_ev.data.ptr = fds;
                    socket_ev.events = EPOLLIN | EPOLLET;
                    res = epoll_ctl(epfd, EPOLL_CTL_ADD, s, &socket_ev);
                    CHECK_ERROR(res, epoll_ctl);
                    go [=] {
                        on_read(fds);
                    };

                    go [=] {
                        on_write(fds);
                    };

                    if (++ connection == 1) {
#if PROFILE
                        ProfilerStart("my.prof");
#endif
                    }
                    goto retry_accept;
                }

                if (ev.events & (EPOLLERR | EPOLLHUP)) {
                    printf("listen error\n");
                    exit(1);
                }
            } else {
                // socket
                int fd = fds->fd;
                if (ev.events & (EPOLLERR | EPOLLHUP)) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);

                    fds->shutdown_ |= shutdown_ep;
                    if (fds->shutdown_ == shutdown_all) {
                        fds->close();
                        continue;
                    }

                    fds->in.TryPush(false);
                    fds->out.TryPush(false);
                } else {
                    if (ev.events & EPOLLIN) {
                        fds->in.TryPush(true);
                    }

                    if (ev.events & EPOLLOUT) {
                        fds->out.TryPush(true);
                    }
                }
            }
        } // end for
    }
}

int main(int argc, char** argv)
{
    go start;
    co_sched.RunLoop();
    return 0;
}
