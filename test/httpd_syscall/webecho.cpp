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

void start()
{
    signal(SIGPIPE, SIG_IGN);

    int epfd = epoll_create(1024);
    CHECK_ERROR(epfd, epoll_create);

    int epfd_layer = epoll_create(1024);
    CHECK_ERROR(epfd_layer, epoll_create);

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
    ev.data.fd = listenfd;
    ev.events = EPOLLOUT | EPOLLIN | EPOLLET;
    res = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    CHECK_ERROR(res, epoll_ctl);

    ev.data.fd = epfd;
    ev.events = EPOLLOUT | EPOLLIN;
    res = epoll_ctl(epfd_layer, EPOLL_CTL_ADD, epfd, &ev);
    CHECK_ERROR(res, epoll_ctl);

    for (;;) {
        static epoll_event evs[1024];

        int n_layer = epoll_wait(epfd_layer, evs, 1024, 20);
        if (n_layer < 0 && errno == EINTR)
            goto retry_epoll_wait;
        CHECK_ERROR(n_layer, epoll_wait);

        if (n_layer == 0)
            continue;

        if (evs[0].data.fd != epfd)
            continue;

retry_epoll_wait:
        int n = epoll_wait(epfd, evs, 1024, 0);
        if (n < 0 && errno == EINTR)
            goto retry_epoll_wait;
        CHECK_ERROR(n, epoll_wait);

        for (int i = 0; i < n; ++i)
        {
            epoll_event & ev = evs[i];
            if (ev.data.fd == listenfd) {
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

                    epoll_event socket_ev;
                    socket_ev.data.fd = s;
                    socket_ev.events = EPOLLIN; // | EPOLLET;
                    res = epoll_ctl(epfd, EPOLL_CTL_ADD, s, &socket_ev);
                    CHECK_ERROR(res, epoll_ctl);

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
                int fd = ev.data.fd;
                if (ev.events & (EPOLLERR | EPOLLHUP)) {
                    closefd(fd);
                } else {
                    if (ev.events & EPOLLIN) {
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
                                closefd(fd);
                                continue;
                            }
                        }

                        if (bytes == 0) {
                            // eof
                            closefd(fd);
                            continue;
                        }

retry_write:
                        bytes = write(fd, html.c_str(), html.length());
                        if (bytes < 0) {
                            if (errno == EINTR)
                                goto retry_write;
                            else if (errno == EAGAIN)
                                goto retry_write;
                            else {
                                closefd(fd);
                                continue;
                            }
                        }

//                        goto retry_read;
                    }
                }
            }
        } // end for
    }
}

int main(int argc, char** argv)
{
    start();
    return 0;
}
