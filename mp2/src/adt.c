/*
 * Author: nonsense
 *
 * Created on Oct 15, 2019
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
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
    // size_t orig = cwnd->window;
    // int state = cwnd->state.state;
    if (ackNum == cwnd->lastAck) { // Dup Ack
        cwnd->state.dupAck(cwnd);
    } else {
        cwnd->state.ack(cwnd, ackNum);
        cwnd->lastAck = ackNum;
    }
    // if (orig > cwnd->window) {
    //     fprintf(stderr, "orig: %d now: %d, state: %d, newState: %d, ack: %d, lastack: %d, dupack: %d, ackthisround: %d\n", 
    //         orig, cwnd->window, state, cwnd->state.state, ackNum, cwnd->lastAck, cwnd->dupAck, cwnd->ackThisRound);
    // }
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

uint32_t __getNextSeq(__attribute__((unused)) Cwnd* cwnd, uint32_t lastSent, __attribute__((unused)) uint32_t highestSeq) {
    // For now only allow increment by 1 from previous sent, i.e., don't jump unless base is updated
    return lastSent + 1;
}

int __allowSend(Cwnd* cwnd, uint32_t base, uint32_t nextSeq) {
    // For now only enable conservative approach. Discard all previously inflight packets
    return (nextSeq - base) < cwnd->window;
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
    return initTimer(&stat->timer);
}

int allowSend(SenderStat* stat, uint32_t nextSeq) {
    return (stat->retrans || __allowSend(&stat->cwnd, stat->sendBase, nextSeq)) && (nextSeq < stat->totalSeq);
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

uint64_t drain(int timerfd) {
    uint64_t round = 0;
    while (read(timerfd, &round, 8) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return round;
        }
        else if (errno != EINTR) {
            perror("Read timer");
            exit(EXIT_FAILURE);
        }
    }
    return round;
}

int initTimer(Timer* timer) {
    timer->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    timer->running = FALSE;
    return timer->fd;
}

int getfd(Timer* timer){
    return timer->fd;
}

void startTimerIfNotRunning(Timer *timer, const struct itimerspec* const spec) {
    if (!timer->running) {
        if (timerfd_settime(timer->fd, 0, spec, NULL)) {
            perror("Start timer");
            exit(EXIT_FAILURE);
        }
        timer->running = TRUE;
    }
}

void startTimer(Timer* timer, const struct itimerspec* const spec) {
    drain(timer->fd);
    if (timerfd_settime(timer->fd, 0, spec, NULL)) {
            perror("Start timer");
            exit(EXIT_FAILURE);
    }
    timer->running = TRUE;
}

void stopTimer(Timer* timer) {
    static const struct itimerspec zero = {0};
    if (timer->running != 0) {
        drain(timer->fd);
        if (timerfd_settime(timer->fd, 0, &zero, NULL)) {
            perror("Stop timer");
            exit(EXIT_FAILURE);
        }
        timer->running = FALSE;
    }
}