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

typedef struct CongestionState CState;
typedef struct CongestionWindow Cwnd;

struct CongestionState {
    char state;
    void (*ack)(Cwnd*, uint32_t);
    void (*dupAck)(Cwnd*);
};

struct CongestionWindow {
    size_t window;
    size_t priorWindow;
    size_t ackThisRound; // Used only in Congestion Avoidance
    size_t ssthresh;
    uint32_t timeoutAck;
    uint32_t lastAck;
    int dupAck;
    CState state;
};

void initCwnd(Cwnd* cwnd);
void ackCwnd(Cwnd* cwnd, uint32_t ackNum);
int confirmThreeDups(Cwnd* cwnd);
void timeoutCwnd(Cwnd* cwnd, uint32_t ack);

typedef struct statistics {
    Cwnd cwnd;
    uint32_t totalSeq;
    uint32_t sendBase;
    uint32_t lastSent;
    uint32_t highestSeq;
    uint16_t lastChunk;
    int timerfd;
    char retrans;
    char recvFin;
} SenderStat;

typedef struct diffStatistics {
    size_t sendBase;
    char threeDups;
} DiffStat;

int allowSend(SenderStat* stat);
int initSenderStat(SenderStat* stat, size_t totalBytes);
uint32_t getNextSeq(SenderStat* stat);
uint32_t updateNextSeq(SenderStat* stat, uint32_t next);

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
    size_t        start;
    size_t        end;
    size_t        length;
} receiverQ;
