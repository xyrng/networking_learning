/*
 * Author: nonsense
 *
 * Created on Oct 6, 2019
 */
#pragma once

#include <stdio.h>
#include <sys/types.h>
#include "constants.h"

typedef struct statistics {
    size_t cwnd = 1;
    size_t estRtt = 20;
    size_t devRtt = 0;
    size_t totalBytes;
    size_t sentBytes;
    size_t sendBase;
    size_t nextSeq;
} tStat;

typedef struct squeue {
    char* buffer[WINDOW] = {0};
    int epollfd = 0;
    size_t start = 0;
    size_t end = 0;
    size_t length = 0;
} senderQ;

typedef struct rqueue {
    char* buffer[WINDOW] = {0};
    size_t start = 0;
    size_t end = 0;
    size_t length = 0;
} receiverQ;

typedef struct rdt_packet {
    uint16_t  src_port;
    uint16_t  dst_port;
    uint32_t  seq_num;
    uint32_t  ack_num;
    uint16_t  payload;         // each time need atoi() to convert
    char      fin_byte;
    char*     data;
} rdt_packet;

void buffer(queue* q, char* buf);
void restartTimer(queue* q, size_t pos);
void remove(queue* q, size_t pos);
