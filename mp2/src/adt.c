/*
 * Author: nonsense
 *
 * Created on Oct 15, 2019
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/timerfd.h>
#include "adt.h"

/*
 * cwnd Impl
 */
void __toSlowStart(Cwnd* cwnd);
void __toCongAvoid(Cwnd* cwnd);
void __toFastRecov(Cwnd* cwnd);
void __toFastRecovRetrans(Cwnd* cwnd);

void __clearDup(Cwnd* cwnd) {
    cwnd->dupAck = 0;
}

void __halveWindow(Cwnd* cwnd) {
    cwnd->ssthresh = cwnd->window > 2 ? cwnd->window >> 1 : 2;
}

void __dupAck(Cwnd* cwnd) {
    if (++(cwnd->dupAck) >= 3) {
        __halveWindow(cwnd);
        // Don't reorder
        cwnd->window = cwnd->ssthresh + 3;
        __toFastRecovRetrans(cwnd);
    }
}

void __ackSlowStart(Cwnd* cwnd, __attribute__((unused)) uint32_t ack) {
    __clearDup(cwnd);
    if (++(cwnd->window) >= cwnd->ssthresh) {
        __toCongAvoid(cwnd);
    }
}

void __toSlowStart(Cwnd* cwnd) {
    cwnd->state.state = SLOW_START;
    cwnd->state.ack = __ackSlowStart;
    cwnd->state.dupAck = __dupAck;
}

void __ackCongAvoid(Cwnd* cwnd, uint32_t ack) {
    __clearDup(cwnd);
    if ((cwnd->ackThisRound += ack - cwnd->lastAck) >= cwnd->window) {
        cwnd->window++;
        cwnd->ackThisRound = 0;
    }
}

void __toCongAvoid(Cwnd* cwnd) {
    cwnd->state.state = CONGEST_AVOID;
    cwnd->state.ack = __ackCongAvoid;
    cwnd->state.dupAck = __dupAck;
    cwnd->ackThisRound = 0; // Always clear before use
}

void __ackFastRecov(Cwnd* cwnd, __attribute__((unused)) uint32_t ack) {
    cwnd->window = cwnd->ssthresh;
    __clearDup(cwnd);
    __toCongAvoid(cwnd);
}

void __dupAckFastRecov(Cwnd* cwnd) {
    cwnd->window++;
}

void __toFastRecov(Cwnd* cwnd) {
    cwnd->state.state = FAST_RECOV;
}

void __toFastRecovRetrans(Cwnd* cwnd) {
    cwnd->state.state = FAST_RECOV_RETRANS;
    cwnd->state.ack = __ackFastRecov;
    cwnd->state.dupAck = __dupAckFastRecov;
}

void initCwnd(Cwnd* cwnd) {
    cwnd->window = 4;
    cwnd->priorWindow = cwnd->lastAck = cwnd->timeoutAck = -1;
    cwnd->dupAck = 0;
    cwnd->ssthresh = 4096;
    __toSlowStart(cwnd);
}

void ackCwnd(Cwnd* cwnd, uint32_t ackNum) {
    if (ackNum == cwnd->lastAck) { // Dup Ack
        cwnd->state.dupAck(cwnd);
    } else {
        cwnd->state.ack(cwnd, ackNum);
        cwnd->lastAck = ackNum;
    }
}

int confirmThreeDups(Cwnd* cwnd) {
    if (cwnd->state.state == FAST_RECOV_RETRANS) {
        __toFastRecov(cwnd);
        return TRUE;
    } else {
        return FALSE;
    }
}

void timeoutCwnd(Cwnd* cwnd, uint32_t ack) {
    if (ack != cwnd->timeoutAck) {
        cwnd->timeoutAck = ack;
        __halveWindow(cwnd);
    }
    __clearDup(cwnd);
    // set to 1 after halve the window
    cwnd->window = 1;
    __toSlowStart(cwnd);
}

uint32_t __getNextSeq(__attribute__((usused)) Cwnd* cwnd, uint32_t lastSent, __attribute__((unused)) uint32_t highestSeq) {
    // For now only allow increment by 1 from previous sent, i.e., don't jump unless base is updated
    return lastSent + 1;
}

int __allowSend(Cwnd* cwnd, uint32_t base, uint32_t lastSent) {
    // For now only enable conservative approach. Discard all previously inflight packets
    return (lastSent - base + 1) < cwnd->window;
    // if (1 || cwnd->state.state == SLOW_START || cwnd->state.state == CONGEST_AVOID) {
    //     return (lastSent - base + 1) < cwnd->window;
    // } else {
    //     return (lastSent - base + 1) < cwnd->priorWindow;
    // }
}

/*
 * Sender Stat Impl
 */
int initSenderStat(SenderStat* stat, size_t totalBytes) {
    stat->sendBase = 0; // Always start expecting 0 th pkt
    stat->lastSent = stat->highestSeq = -1;
    stat->totalSeq = 1 + (totalBytes - 1) / MAX_PAYLOAD_LEN;
    if (!(stat->lastChunk = totalBytes % MAX_PAYLOAD_LEN)) {
        stat->lastChunk = MAX_PAYLOAD_LEN;
    }
    stat->retrans = FALSE;
    stat->recvFin = FALSE;
    initCwnd(&stat->cwnd);
    return stat->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
}

int allowSend(SenderStat* stat) {
    return stat->retrans || __allowSend(&stat->cwnd, stat->sendBase, stat->lastSent);
}

uint32_t getNextSeq(SenderStat* stat) {
    return stat->retrans || stat->lastSent < stat->sendBase ?
                stat->sendBase : __getNextSeq(&stat->cwnd, stat->lastSent, stat->highestSeq);
}

uint32_t updateNextSeq(SenderStat* stat, uint32_t lastSent) {
    if ((stat->lastSent = lastSent) == stat->sendBase) {
        stat->retrans = FALSE;
    }
    if (stat->highestSeq < lastSent) {
        stat->highestSeq = lastSent;
    }
    return getNextSeq(stat);
}
