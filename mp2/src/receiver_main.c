/*
 * File:   receiver_main.c
<<<<<<< HEAD
 * Author:
||||||| merged common ancestors
 * Author: 
=======
 * Author: nonsense
>>>>>>> shared
 *
 * Created on Oct 6, 2019
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
<<<<<<< HEAD
#include <adt.h>

||||||| merged common ancestors

=======
#include "constants.h"
>>>>>>> shared


struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

// data should be on the heap
void parse_packet(char* buff, rdt_packet* packet, ssize_t size) {

}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
<<<<<<< HEAD

||||||| merged common ancestors
    
=======
>>>>>>> shared
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


<<<<<<< HEAD
	/* Now receive data and send acknowledgements */
    // receive from socket

    char buffer[4096];
    uint32_t ack_num = 0;
    uint32_t last_payload = 0;
    FILE *fd = fopen(destinationFile, 'a');
    do {
        ssize_t size = read(s, &buffer, 4096);
        rdt_packet *packet;
        parse_packet(&buffer, packet, size);
        // get seq num
        if (packet->seq_num == last_seq_num + last_payload) {
            // append write to file
            do () {
                fwrite();
            } while();
            // send ack_num
            last_payload = (uint32_t) packet->payload;
            ack_num = packet->seq_num + last_payload;

        } else {
            // mem_map to buffer
            // send dupc_ack_num
        }
    } while(1)

    fclose(fp);
||||||| merged common ancestors
	/* Now receive data and send acknowledgements */    

=======
    /* Now receive data and send acknowledgements */
    while (1) {

    }

>>>>>>> shared
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
