#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <sys/types.h>
#include <error.h>
#include <errno.h>

int read_all(int fd, void* buffer, size_t bytes_to_read) {
    size_t bytes_read = 0;
    size_t this_read = 0;
    char *buf = buffer;
    while (bytes_to_read > 0) {
        if ((this_read = read(fd, buf + bytes_read, bytes_to_read)) == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "Warning: Depleted on Nonblocking Read\n");
                return 0;
            } else {
                perror("read_all");
                return -1;
            }
        }
        bytes_to_read -= this_read;
        bytes_read += this_read;
    }
}

int main() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fileno(stdin);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fileno(stdin), &ev);

    struct itimerspec spec = {0};
    spec.it_value.tv_sec = 1;
    spec.it_interval.tv_sec = 3;
    if (timerfd_settime(timer_fd, 0, &spec, NULL) != 0) {
        perror("timerfd_settime");
        exit(EXIT_FAILURE);
    };

    sleep(6);

    struct epoll_event evs[100];
    int n = 10;
    while (n--) {
        int nfds;
        while ((nfds = epoll_wait(epoll_fd, evs, 100, -1)) == -1) {
            if (errno != EINTR) {
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            }
        }

        for (struct epoll_event *it = evs, *end = evs + nfds; it < end; ++it) {
            int fd = it->data.fd;
            char exp[100] = {0};
            read_all(fd, &exp, 6);
            printf("Caught %s time(s) timeout\n", exp);
            // struct itimerspec ospec;
            // spec.it_value.tv_sec <<= 1;
            // timerfd_settime(fd, 0, &spec, &ospec);
            // printf("Current Setting: timeout in %ld secs %ld nsecs\n", ospec.it_value.tv_sec, ospec.it_value.tv_nsec);
        }
    }
    return 0;
}
