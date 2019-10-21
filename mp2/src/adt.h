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

typedef struct CongestionWindow {
    size_t window;
    size_t ackThisRound;
    size_t ssthresh;
} Cwnd;

size_t getCwnd(Cwnd* cwnd);
int _isFull(SenderStat* stat, Cwnd* cwnd);

typedef struct statistics {
    Cwnd cwnd;
    size_t recentAcks[3];
    size_t unacked;
    size_t estRttMs;
    size_t devRttMs;
    size_t totalBytes;
    size_t sentBytes;
    size_t sendBase;
    size_t nextSeq;
    int timerfd;
} SenderStat;

int ack(SenderStat* stat);
int isCwndFull(SenderStat* stat);

int initSenderStat(SenderStat* stat, size_t totalBytes);

typedef struct timedSlot {
    char* chunk;
    uint16_t length;
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
