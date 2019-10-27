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

#define evsLen 2

struct sockaddr_in si_other;
int s, slen;

const struct itimerspec defaultSpec = {{0}, {0, 20000000}}; // 20 ms RTT

void diep(char *s) {
    perror(s);
    exit(EXIT_FAILURE);
}

void warn(const char* const s) {
    fprintf(stderr, "[WARNING]: %s", s);
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
    tev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timerfd, &tev)) {
        diep("Add Socket to Epoll");
    }

    return epoll_fd;
}

int recvAll(int sockfd, ack_packet* const pkts) {
    struct sockaddr_storage addr_container = {0};
    socklen_t addr_len = sizeof addr_container;
    struct sockaddr* their_addr = &addr_container;
    size_t pkt_len = sizeof(ack_packet);

    for (ack_packet* it = pkts, *end = pkts + WINDOW; it < end; ++it) {
        size_t numbytes;
        while ((numbytes = recvfrom(sockfd, it, pkt_len, 0, 
                their_addr, &addr_len)) == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EWOULDBLOCK | EAGAIN) {
                return it - pkts;
            }
            diep("recvfrom");
        }
        struct sockaddr_in* cast = their_addr;
        if (numbytes != pkt_len) {
            warn("Ack corrupted");
            --it;
        } else if (cast->sin_port != si_other.sin_port || cast->sin_addr.s_addr != si_other.sin_addr.s_addr) {
            warn("Ignoring packets from nonrelevant addr");
            --it;
        }
    }
    return WINDOW;
};

uint64_t drain(int timerfd) {
    uint64_t round = 0;
    while (read(timerfd, &round, 8) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return round;
        }
        else if (errno != EINTR) {
            diep("read timerfd");
        }
    }
    return round;
}

void ackAll(ack_packet* pkts, size_t len, SenderStat* const currStat, DiffStat* const pending) {
    pending->sendBase = currStat->sendBase;
    for (ack_packet* it = pkts, *end = pkts + len; it < end; ++it) {
        uint32_t ack = it->ack_num;
        if (ack >= pending->sendBase) {
            pending->sendBase = ack;
            currStat->recvFin |= it->fin_byte;
            // TODO: Move to outside?
            ackCwnd(&currStat->cwnd, ack);
        } else {
            warn("Out of window ack from receiver. Ignored");
        }
    }
    pending->threeDups = confirmThreeDups(&currStat->cwnd);
}

void merge(SenderStat* const stat, const DiffStat* const pending, int hasTimeout) {
    stat->retrans = pending->threeDups;
    if (stat->sendBase < pending->sendBase) { // Advanced, ignore old timer
        stat->sendBase = pending->sendBase;
        if (!stat->retrans) { 
            if (stat->sendBase < stat->nextSeq) { // have unacked
                startTimer(stat->timerfd);
            } else {
                stopTimer(stat->timerfd);
            }
        }
    } else if (!stat->retrans && (stat->retrans = hasTimeout)) {
        timeoutCwnd(&stat->cwnd);
    }
}

int sendpackt(const char* const file, SenderStat* stat) {
    for (const char *it = file + MAX_PAYLOAD_LEN * stat->nextSeq,
                    *end = file + MAX_PAYLOAD_LEN * (stat->sendBase + getCwnd(&stat->cwnd));
                    it < end; ++it) {
    };
    return 0;
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
    int epollFd = initEpollPool(&s, EPOLLIN | EPOLLOUT | EPOLLET, &stat.timerfd);

    if (sendpackt(mFile, &stat)) {
        diep("sendpacket");
    }

    struct epoll_event evs[evsLen];
    ack_packet pktBuffer[WINDOW];
    while(stat.sendBase < stat.totalSeq) {
        int nfds;
        while ((nfds = epoll_wait(epollFd, evs, evsLen, -1)) == -1) {
            if (errno != EINTR) {
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            }
        }

        int hasTimeout = FALSE;
        int nPackets = 0;
        for (struct epoll_event *it = evs, *end = evs + nfds; it < end; ++it) {
            if (it->data.fd == s) { // Reading events of socket
                if (it->events & EPOLLIN) { // Receive Ack
                    nPackets = recvAll(s, pktBuffer);
                }

                if (it->events & EPOLLOUT) {
                    sockWritable = 1;
                }
            } else { 
                uint64_t round = drain(it->data.fd);
                if (round >= 1) {
                    hasTimeout = TRUE;
                } 
                if (round > 1) {
                    warn("Timeout multiple times");
                }
            }
        }

        if (nPackets) {
            DiffStat diff;
            ackAll(pktBuffer, nPackets, &stat, &diff);
            merge(&stat, &diff, hasTimeout);
        }

        // Process stat diff
        if (sockWritable) {
            if (stat.retrans) {
                // retrans(mFile, &stat);
            }
            if (!allowSend(&stat)) {
                // sendpackt(mFile, &stat)
            }
        }
    }

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


