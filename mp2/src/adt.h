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

typedef struct timer {
    int fd;
    char running;
} Timer;

int initTimer(Timer* timer);
int getfd(Timer* timer);
uint64_t drain(int timerfd);
void startTimerIfNotRunning(Timer* timer, const struct itimerspec* const spec);
void startTimer(Timer* timer, const struct itimerspec* const spec);
void stopTimer(Timer* timer);

typedef struct statistics {
    Cwnd cwnd;
    uint32_t totalSeq;
    uint32_t sendBase;
    uint32_t lastSent;
    uint32_t highestSeq;
    uint16_t lastChunk;
    Timer timer;
    char retrans;
    char recvFin;
} SenderStat;

typedef struct diffStatistics {
    size_t sendBase;
    char threeDups;
} DiffStat;

int allowSend(SenderStat* stat, uint32_t next);
int initSenderStat(SenderStat* stat, size_t totalBytes);
uint32_t getNextSeq(SenderStat* stat);
uint32_t updateNextSeq(SenderStat* stat, uint32_t lastSent);

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
    char      data[MAX_PAYLOAD_LEN];
} rdt_packet;

typedef struct rqueue {
    rdt_packet    buffer[WINDOW];
    size_t        buflen[WINDOW];      // whether there is a packet
    size_t        start;
    size_t        end;
    size_t        length;
} receiverQ;
