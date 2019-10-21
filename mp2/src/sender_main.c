/*
 * File:   sender_main.c
 * Author: nonsense
 *
 * Created on Oct 6, 2019
 */
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "constants.h"
#include "adt.h"

#define evsLen 1 << 8

struct sockaddr_in si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(EXIT_FAILURE);
}

char* mapFile(const char* filename, size_t size) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        diep("open");
    void *ptr = mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_NORESERVE | MAP_POPULATE, fd, 0);
    if (ptr == MAP_FAILED) {
        diep("mmap");
    }
    close(fd);
    return (char *) ptr;
}

int initEpollPool(int sockfd, uint32_t sockEvent, int timerfd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        diep("epoll_create1");
    }

    struct epoll_event sev;
    sev.data.fd = sockfd;
    sev.events = sockEvent;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &sev)) {
        diep("Add Socket to Epoll");
    }

    struct epoll_event tev;
    tev.data.fd = timerfd;
    tev.events = EPOLLET | EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timerfd, &tev)) {
        diep("Add Socket to Epoll");
    }

    return epoll_fd;
}

int updateSockEvent(int epoll_fd, int* socket, uint32_t sockEvent) {
    struct epoll_event sev;
    sev.data.ptr = socket;
    sev.events = sockEvent;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, *socket, &sev);
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    struct stat fileStat = {0};
    if (stat(filename, &fileStat)) {
        diep("stat");
    }
    char* mFile = mapFile(filename, bytesToTransfer);

    /* Determine how many bytes to transfer */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP)) == -1) {
        diep("socket");
    }
    int sockWritable = 1;

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Send data and receive acknowledgements on s*/
    SenderStat stat; initSenderStat(&stat, bytesToTransfer);
    SenderQ q; init(&q);
    int epollFd = initEpollPool(&s, EPOLLIN | EPOLLOUT | EPOLLET, &stat.timerfd);

    size_t totalBuffered = buffer(&q, &mFile, bytesToTransfer);
    if (sendpackt(&q, &stat)) {
        diep("sendpacket");
    }

    struct epoll_event evs[evsLen];
    while(stat.sentBytes < bytesToTransfer) {
        int nfds;
        while ((nfds = epoll_wait(epollFd, evs, evsLen, -1)) == -1) {
            if (errno != EINTR) {
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            }
        }

        for (struct epoll_event *it = evs, *end = evs + nfds; it < end; ++it) {
            if (it->data.fd == s) { // Reading events of socket
                if (it->events & EPOLLIN) { // Receive Ack
                    // TODO: Read Data from socket
                    struct sockaddr_in s_other;
                }

                if (it->events & EPOLLOUT) {
                    sockWritable = 1;
                }
            } else { // TIMEOUT EVENT!!!!! BOOOOM
                uint64_t round;
                read_all(it->data.fd, &round, 8);
            }
        }

        // Process stat diff
        if (sockWritable && !isCwndFull(&stat) && !isQFull(&q)) {
            // Lazy buffering
            size_t thisBuffered = buffer(&q, &mFile, bytesToTransfer - totalBuffered);
            totalBuffered += thisBuffered;

            // sendpackt(&q, &stat)
        }
    }
    // do {
    //     if (/* buffer not full*/) {
    //         buffer(q);
    //     }
    //     sendpackt(stat.cwnd);
    //     acknum = waitforack(fd)
    //     if （/*dup ack or timeout */) {
    //         retransmit()
    //         cut_cwnd()
    //     } else {
    //         // update stat:
    //         // update cwnd
    //         // update rtt
    //         // update sendbase
    //     }
    // } while (stat.sentBytes < bytesToTransfer);


    printf("Closing the socket\n");
    close(s);
    return;

}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


