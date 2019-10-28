/*
 * File:   receiver_main.c
 * Author: nonsense
 *
 * Created on Oct 6, 2019
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <error.h>
#include "adt.h"
#include "constants.h"



struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

void warn(const char* const s) {
    fprintf(stderr, "[WARNING]: %s", s);
}

// TODO: check stack mem problem
void build_ack_packet(ack_packet *ack_packet, rdt_packet *packet, uint32_t new_ack_num) {
    packet->seq_num = packet->ack_num;
    ack_packet->ack_num = new_ack_num;
    ack_packet->fin_byte = packet->fin_byte;
}

int write_to_file(int fd, rdt_packet *packet) {
    ssize_t writed_size = 0;
    ssize_t total_size = (ssize_t) packet->payload;
    do {
        ssize_t round = write(fd, packet->data + writed_size, total_size - writed_size);
        if (round == -1) {
            perror("Error");
            exit(1);
        }
        writed_size += round;
    } while (writed_size < total_size);
    // fprintf(stderr, "wriiten %d\n", packet->seq_num);
    return 0;
}

void send_ack_packet(int socket_fd, ack_packet *ack_pkt) {
    while (sendto(socket_fd, ack_pkt, sizeof(ack_packet), 0,
               (struct sockaddr *)&si_other, sizeof(si_other)) == -1) {
        if (errno == EINTR) {
            continue;
        }
        diep("[Error]send_ack_packet");
    }
}

int check_and_sent_buffer(int write_fd, int socket_fd, uint32_t *seq_num, receiverQ *rec_queue, rdt_packet *pkt) {
    int retval = 0;
    uint32_t ret_seq_num = *seq_num;
    // send accumulative ack
    ack_packet ack_pkt = {0};
    uint32_t last_ack_num = ret_seq_num;
    while (rec_queue->buflen[last_ack_num % WINDOW] == 1) {
        last_ack_num++;
    }

    if (last_ack_num > ret_seq_num) {
        rdt_packet *last_pkt = rec_queue->buffer + ((last_ack_num - 1) % WINDOW);
        build_ack_packet(&ack_pkt, last_pkt, last_ack_num);
        send_ack_packet(socket_fd, &ack_pkt);
    } else {
        build_ack_packet(&ack_pkt, pkt, last_ack_num);
        send_ack_packet(socket_fd, &ack_pkt);
    }

    // write buffer to file
    rdt_packet *temp_pkt = rec_queue->buffer + (ret_seq_num % WINDOW);
    while (rec_queue->buflen[ret_seq_num % WINDOW] == 1) {
        // write that packet to the file
        if (temp_pkt->fin_byte == 1) {
            retval = 1;
        }
        if (write_to_file(write_fd, temp_pkt) != 0) {
            fprintf(stderr, "Write to file Failure.\n");
            exit(1);
        }
        rec_queue->buflen[ret_seq_num % WINDOW] = 0;
        ret_seq_num++;
        memset(temp_pkt, 0, sizeof(rdt_packet));
        temp_pkt = rec_queue->buffer + (ret_seq_num % WINDOW);
    }
    *seq_num = ret_seq_num;
    return retval;
}

void map_to_queue(receiverQ *queue, rdt_packet *packet) {
    uint32_t seq_num = packet->seq_num % WINDOW;
    memcpy(queue->buffer + seq_num, packet, sizeof(rdt_packet));
    queue->buflen[seq_num] = 1;
}

// TODO: check
int recvPacket(int sockfd, rdt_packet* const packet) {
    struct sockaddr_storage addr_container = {0};
    socklen_t addr_len = sizeof(addr_container);
    struct sockaddr *from_addr = (struct sockaddr *) &addr_container;

    // fprintf(stderr,"recvPacket at line %d\n", __LINE__);
    // fprintf(stderr,"sockfd: %d\npacket_addr: %p", sockfd, packet);
    // fprintf(stderr,"packet->seq_num: %d\npacket->ack_num: %d", packet->seq_num, packet->ack_num);
    size_t pkt_len = sizeof(rdt_packet);
    size_t numbytes;
    while ((numbytes = recvfrom(sockfd, packet, pkt_len, 0, from_addr, &addr_len)) == -1) {
        if (errno == EINTR) {
            continue;
        }
        diep("recvfrom");
    }
    // fprintf(stderr, "Received %zu bytes; Expect %zu bytes.\n", numbytes, pkt_len);
    if (numbytes != pkt_len) {
        warn("Packet Reading Failer [Size]");
        return -1;
    }
    struct sockaddr_in* cast = (struct sockaddr_in *) from_addr;
    if (packet->seq_num == 0) {
        si_other.sin_family = AF_INET;
        si_other.sin_port = cast->sin_port;
        si_other.sin_addr.s_addr = cast->sin_addr.s_addr;
    }
    // TODO: check
    if (cast->sin_port != si_other.sin_port || cast->sin_addr.s_addr != si_other.sin_addr.s_addr) {
        warn("Ignoring packets from nonrelevant addr");
        return -1;
    }
    return 0;
}

void wait_for_break(int socket_fd, uint32_t last_ack_num) {
    ack_packet final_ack = {0};
    final_ack.fin_byte = 1;
    final_ack.ack_num = last_ack_num;
    final_ack.seq_num = 0;
    for (int i = 0; i < 45; i++) {
        sendto(socket_fd, &final_ack, sizeof(final_ack), 0,
            (struct sockaddr *)&si_other, sizeof(si_other));
        usleep(20000);
        // TODO: how to break
    }
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */
    receiverQ rec_queue = {0};
    uint32_t last_ack_num = 0;
    int recv_fin_byte = 0;
    int write_file_fd;
    if ((write_file_fd = open(destinationFile, O_WRONLY | O_TRUNC | O_CREAT, 0666)) != -1) {
        do {
            rdt_packet packet = {0};
            ack_packet ack_pkt = {0};
            if (recvPacket(s, &packet) != 0){
                fprintf(stderr, "Parsing Packet Failure.\n");
                if (last_ack_num == 0) {
                    continue;
                }
                // send dup ack
                build_ack_packet(&ack_pkt, &packet, last_ack_num);
                send_ack_packet(s, &ack_pkt);
                continue;
            }
            // fprintf(stderr,"received at line %d\n", __LINE__);
            // write to file or save to queue
            if (packet.seq_num == last_ack_num) {
                if (write_to_file(write_file_fd, &packet) != 0) {
                    // TODO: check this situation handling.
                    fprintf(stderr, "Write to file Failure.\n");
                    exit(1);
                } else {
                    last_ack_num = packet.seq_num + 1;
                    if (packet.fin_byte == 1) {
                        recv_fin_byte = 1;
                        close(write_file_fd);
                        wait_for_break(s, last_ack_num);
                        break;
                    }
                    if (check_and_sent_buffer(write_file_fd, s, &last_ack_num, &rec_queue, &packet) == 1) {
                        recv_fin_byte = 1;
                        close(write_file_fd);
                        wait_for_break(s, last_ack_num);
                        break;
                    }
                    // build_ack_packet(&ack_pkt, &packet, last_ack_num);
                    // send_ack_packet(s, &ack_pkt);
                }
            } else if (packet.seq_num > last_ack_num) {
                map_to_queue(&rec_queue, &packet);
                build_ack_packet(&ack_pkt, &packet, last_ack_num);
                send_ack_packet(s, &ack_pkt);
            } else {
                build_ack_packet(&ack_pkt, &packet, last_ack_num);
                send_ack_packet(s, &ack_pkt);
            }
        } while(1);
    } else {
        perror("Error");
    }

    if (recv_fin_byte != 1) {
        close(write_file_fd);
    }
    close(s);
    printf("%s received.", destinationFile);
    return;
}

/*
 *
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}
