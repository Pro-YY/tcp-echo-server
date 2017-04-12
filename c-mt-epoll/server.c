#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <pthread.h>

#ifndef NDEBUG  /* debug */
#define debug_print(...) \
    do { fprintf(stderr, __VA_ARGS__); fflush(stderr); } while (0)
#else
#define debug_print
#endif

#define handle_error(...) \
    do { fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); } while (0)

#define NUM_THREADS 4
#define PORT 7000
#define MAX_EVENTS_SIZE 1024
#define MAX_BUFFER_SIZE 16
#define MAX_LINE_SIZE 1024

typedef struct thread_info {
    pthread_t thread_id;
    int thread_num;
} thread_info_t;

typedef struct line {
    int fd;
    char buf[MAX_LINE_SIZE];
    size_t size;
} line_t;

static void *worker_routine(void *data) {
    struct thread_info *tinfo = (struct thread_info *)data;
    int tnum = tinfo->thread_num;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int sfd = -1, cfd = -1, epfd = -1;
    struct epoll_event *events;
    struct epoll_event event;
    int s = -1;
    int nfds, i;
    ssize_t nrecv, nsend;

    // init sockaddr struct
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    // create (non-blocking)
    sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sfd < 0) handle_error("socket: %s\n", strerror(errno));
    // set port reuse, or `Address already in use`
    s = setsockopt(sfd, SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT, &(int){1}, sizeof(int));
    if (s == -1) handle_error("setsockopt: %s\n", strerror(errno));
    // bind
    s = bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (s == -1) handle_error("bind: %s\n", strerror(errno));
    // listen
    s = listen(sfd, SOMAXCONN);
    if (s == -1) handle_error("listen: %s\n", strerror(errno));
    fprintf(stdout, "worker[%d] is listening on: %d\n", tinfo->thread_num, PORT);

    // create epoll fd
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) handle_error("epoll_create1: %s\n", strerror(errno));
    // register server listen socket
    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;    // Edge-Triggered
    s = epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1) handle_error("epoll_ctl: %s\n", strerror(errno));
    // alloc epoll events buffer
    events = calloc(MAX_EVENTS_SIZE, sizeof(event));
    if (events == NULL) handle_error("calloc epoll events\n");

    // event loop
    for (;;) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS_SIZE, -1);
        for (i = 0; i < nfds; i++) {
            if (events[i].data.fd == sfd && (events[i].events & EPOLLIN)) {
                // listening socket event, new client connected
                while (1) {
                    // keep read(accept) until eagain
                    client_len = sizeof(client_addr);
                    cfd = accept4(sfd, &client_addr, &client_len,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);    // non-blocking
                    if (cfd == -1) {
                        if (errno == EAGAIN) {
                            // all connections accepted
                            debug_print("accept again: %s\n", strerror(errno));
                        }
                        else {
                            handle_error("accept: %s\n", strerror(errno));
                        }
                        break;  // exit accept new connection for this run
                    }
                    printf("worker[%d] accepted connection on %d\n", tnum, cfd);

                    // register the new connected fd
                    event.data.ptr = (struct line *)malloc(sizeof(struct line));
                    if (!event.data.ptr) handle_error("malloc event\n");
                    ((struct line *)event.data.ptr)->fd = cfd;
                    event.events = EPOLLIN | EPOLLET; // Edge-Triggered
                    s = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &event);
                    if (s == -1) handle_error("epoll_ctl: %s\n", strerror(errno));
                }
            }
            else if (events[i].events & EPOLLIN) {
                // recv data
                // reset line buffer of event
                memset(((line_t *)events[i].data.ptr)->buf, 0, MAX_LINE_SIZE);
                ((line_t *)events[i].data.ptr)->size = 0;
                // keep read(recv) until eagain
                nrecv = -1;
                while ((nrecv = recv(
                            ((line_t *)events[i].data.ptr)->fd,
                            ((line_t *)events[i].data.ptr)->buf +
                                    ((line_t *)events[i].data.ptr)->size,
                            MAX_BUFFER_SIZE, 0
                        )
                ) > 0) {
                    // got buffer filled
                    // write(1, ((line_t *)events[i].data.ptr)->buf + ((line_t *)events[i].data.ptr)->size, nrecv);
                    // printf("\n");
                    ((line_t *)events[i].data.ptr)->size += nrecv;
                }
                if (nrecv == 0) {
                    debug_print("recv zero: %d\n", ((line_t *)events[i].data.ptr)->fd);
                }
                else if (nrecv == -1 && errno == EAGAIN) {
                    // what we want
                    //debug_print("all received with eagain: %s\n", strerror(errno));
                }
                else {
                    if (errno == ECONNRESET) {
                        printf("client disconnected\n");
                    }
                    else {
                        handle_error("recv: %s\n", strerror(errno));
                    }
                }

                // got whole line
                debug_print("worker[%d](%d) recv: %lu bytes\n", tnum,
                        ((line_t *)events[i].data.ptr)->fd,
                        ((line_t *)events[i].data.ptr)->size);
                // write(1, ((line_t *)events[i].data.ptr)->buf, ((line_t *)events[i].data.ptr)->size);

                // modify event to send
                events[i].events = EPOLLOUT | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_MOD,
                        ((line_t *)events[i].data.ptr)->fd, &events[i]);
            }
            else if (events[i].events & EPOLLOUT) {
                // recv zero or more data
                // just close when recv zero
                if (((line_t *)events[i].data.ptr)->size == 0) {
                    printf("worker[%d](%d) close\n", tnum,
                            ((struct line *)events[i].data.ptr)->fd);
                    close(((line_t *)events[i].data.ptr)->fd);
                    free(events[i].data.ptr);
                }
                else {
                    nsend = -1;
                    if ((nsend = send(
                            ((line_t *)events[i].data.ptr)->fd,
                            ((line_t *)events[i].data.ptr)->buf,
                            ((line_t *)events[i].data.ptr)->size,
                            0
                        )
                    ) > 0 ) {
                        debug_print("worker[%d](%d) send: %lu bytes\n", tnum,
                                ((line_t *)events[i].data.ptr)->fd, nsend);
                    }
                    else {
                        handle_error("send: %s\n", strerror(errno));
                    }

                    // after send line, restore to read
                    events[i].events = EPOLLIN | EPOLLET;
                    epoll_ctl(epfd, EPOLL_CTL_MOD,
                            ((line_t *)events[i].data.ptr)->fd, &events[i]);
                }
            }
            else {
                printf("unknown event occured %d\n", events[i].data.fd);
            }
        } // end events traversal
    } // end event loop
}

int main(int argc, char *argv[]) {
    pthread_t t;
    struct thread_info *tinfo = NULL;
    int s = -1, i = -1;

    tinfo = calloc(NUM_THREADS, sizeof(struct thread_info));
    if (!tinfo) handle_error("calloc: tinfo\n");

    // create threads
    for (i = 0; i < NUM_THREADS; i++) {
        tinfo[i].thread_num = i + 1;
        s = pthread_create(&tinfo[i].thread_id, NULL, worker_routine, &tinfo[i]);
        if (s != 0) handle_error("pthread_create\n");
    }

    // never reach here
    // join all threads
    for (i = 0; i < NUM_THREADS; i++) {
        s = pthread_join(tinfo[i].thread_id, NULL);
        if (s != 0) handle_error("pthread_join\n");
    }

    free(tinfo);
    return EXIT_SUCCESS;
}
