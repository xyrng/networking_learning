/*
 * Author: nonsense
 *
 * Created on Oct 6, 2019
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "constants.h"

typedef struct CongestState {
    char state;
    void (*ack)(Cwnd*);
    void (*dupAck)(Cwnd*);
} CState;

typedef struct CongestionWindow {
    size_t window;
    size_t ackThisRound; // Used only in Congestion Avoidance
    size_t ssthresh;
    size_t lastAck;
    size_t dupAck;
    CState state;
} Cwnd;

void initCwnd(Cwnd* cwnd);
void ackCwnd(Cwnd* cwnd, uint32_t ackNum);
int confirmRetrans(Cwnd* cwnd);
void timeoutCwnd(Cwnd* cwnd);
size_t getCwnd(Cwnd* cwnd);

typedef struct statistics {
    Cwnd cwnd;
    size_t totalBytes;
    size_t sentBytes;
    size_t sendBase;
    size_t nextSeq;
    int timerfd;
    char retrans;
    char recvFin;
} SenderStat;

typedef struct diffStatistics {
    Cwnd newCwnd;
    size_t sendBase;
    char recvFin;
} DiffStat;

int isCwndFull(SenderStat* stat);
int initSenderStat(SenderStat* stat, size_t totalBytes);

typedef struct timedSlot {
    char* chunk;
    uint16_t length;
    int timed;
    struct timespec timestamp;
} timedSlot;

int start(timedSlot* slot);
double getLapse(timedSlot* slot, struct timespec now);
void finish(timedSlot* slot);
int isFinished(timedSlot* slot);

typedef struct queue {
    timedSlot slots[WINDOW];
    size_t start;
    size_t end;
    size_t length;
} SenderQ;

void initSenderQ(SenderQ* q);
int isQFull(SenderQ* q);
size_t buffer(SenderQ* q, const char** const bufferPtr, const size_t toBuffer);
int sweep(SenderQ* q);

typedef struct Summary {
    int threeDup;
    size_t ackBases[3]; // Track recent 3 acks
} AckSummary;

typedef struct ack_packet {
    uint32_t seq_num;
    uint32_t ack_num;
    char     fin_byte;
} ack_packet;

typedef struct rdt_packet {
    uint32_t  seq_num;
    uint32_t  ack_num;
    uint16_t  payload;              //data size
    char      fin_byte;
    char      data[1024];
} rdt_packet;

typedef struct rqueue {
    rdt_packet    buffer[WINDOW];
    size_t        buflen[WINDOW];      // length of each packet
    size_t        start = 0;
    size_t        end = 0;
    size_t        length = 0;          // queue length
} receiverQ;
