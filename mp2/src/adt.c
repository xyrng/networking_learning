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
 * Timed Slots Impl
 */
int start(timedSlot* slot) {
    return clock_gettime(CLOCK_MONOTONIC, &slot->timestamp);
}

void finish(timedSlot* slot) {
    slot->chunk = NULL;
    slot->length = 0;
    slot->timestamp.tv_sec = slot->timestamp.tv_nsec = 0;
}

int isFinished(timedSlot* slot) {
    return !(slot->chunk || slot->length || slot->timestamp.tv_nsec || slot->timestamp.tv_sec);
}

/* 
 * Sender Queue Impl
 */
void init(SenderQ* q) {
    q-> length = q-> start = q->end = 0;
    for (Slot *it = q->slots, *end = q->slots + WINDOW; it < end; ++it) {
        it->chunk = NULL;
        it->length = 0;
    }
}

int isQFull(SenderQ* q) {
    return !(WINDOW > q->length);
}

// Return bytes buffered
size_t buffer(SenderQ* q, const char** const bufferPtr, const size_t toBuffer) {
    char* buffer = *bufferPtr;
    size_t buffered = 0;
    size_t available = WINDOW - q->length;
    if (available > 0) {
        size_t chunks = toBuffer / MAX_PAYLOAD_LEN; // Always truncate the end to avoid seg fault
        if (chunks > available) {
            chunks = available;
        }
        q->length += chunks;
        buffered += MAX_PAYLOAD_LEN * chunks;
        for (; chunks && buffer; chunks--, buffer += MAX_PAYLOAD_LEN, 
                q->end = (q->end + 1) % WINDOW) {
            q->slots[q->end].chunk = buffer;
            q->slots[q->end].length = MAX_PAYLOAD_LEN;
        }
        if (!buffer) {
            fprintf(stderr, "NULL buffer");
            exit(EXIT_FAILURE);
        }
        if (q->length < WINDOW && toBuffer > buffered) {
            if (toBuffer <= MAX_PAYLOAD_LEN + buffered) {
                q->slots[q->end].chunk = buffer;
                q->slots[q->end].length = toBuffer - buffered;
                buffer += toBuffer - buffered;
                return toBuffer;
            } else {
                fprintf(stderr, "Something is WRONG!");
                exit(EXIT_FAILURE);
            }
        }
    }
    *bufferPtr = buffer;
    return buffered;
}

int sweep(SenderQ* q) {
    for (; q->length && isFinished(q->slots + q->start); q->length--, q->start = (q->start + 1) % WINDOW);
}

/*
 * Sender Stat Impl
 */
int initSenderStat(SenderStat* stat, size_t totalBytes) {
    stat->sendBase = stat->nextSeq = 0;
    stat->totalSeq = 1 + (totalBytes - 1) / MAX_PAYLOAD_LEN;
    stat->retrans = FALSE;
    stat->recvFin = FALSE;
    init(&stat->cwnd);
    return stat->timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
}

int allowSend(SenderStat* stat) {
    return stat->nextSeq - stat->sendBase < getCwnd(&stat->cwnd);
}

/*
 * cwnd Impl
 */
void _clearDup(Cwnd* cwnd) {
    cwnd->dupAck = 0;
}

void _halveWindow(Cwnd* cwnd) {
    cwnd->ssthresh = cwnd->window >> 1;
}

void _dupAck(Cwnd* cwnd) {
    if (++(cwnd->dupAck) >= 3) {
        _halveWindow(cwnd);
        // Don't reorder
        cwnd->window = cwnd->ssthresh + 3;
        _toFastRecovRetrans(cwnd);
    }
}

void _ackSlowStart(Cwnd* cwnd, uint32_t ack) {
    _clearDup(cwnd);
    if (++(cwnd->window) >= cwnd->ssthresh) {
        _toCongAvoid(cwnd);
    }
}

void _toSlowStart(Cwnd* cwnd) {
    cwnd->state.state = SLOW_START;
    cwnd->state.ack = _ackSlowStart;
    cwnd->state.dupAck = _dupAck;
}

void _ackCongAvoid(Cwnd* cwnd, uint32_t ack) {
    _clearDup(cwnd);
    if ((cwnd->ackThisRound += ack - cwnd->lastAck) >= cwnd->window) {
        cwnd->window++;
        cwnd->ackThisRound = 0;
    }
}

void _toCongAvoid(Cwnd* cwnd) {
    cwnd->state.state = CONGEST_AVOID;
    cwnd->state.ack = _ackCongAvoid;
    cwnd->state.dupAck = _dupAck;
    cwnd->ackThisRound = 0; // Always clear before use
}

void _ackFastRecov(Cwnd* cwnd, uint32_t ack) {
    cwnd->window = cwnd->ssthresh;
    _clearDup(cwnd);
    _toCongAvoid(cwnd);
}

void _dupAckFastRecov(Cwnd* cwnd) {
    cwnd->window++;
}

void _toFastRecov(Cwnd* cwnd) {
    cwnd->state.state = FAST_RECOV;
}

void _toFastRecovRetrans(Cwnd* cwnd) {
    cwnd->state.state = FAST_RECOV_RETRANS;
    cwnd->state.ack = _ackFastRecov;
    cwnd->state.dupAck = _dupAckFastRecov;
}

void initCwnd(Cwnd* cwnd) {
    cwnd->window = 4;
    cwnd->lastAck = -1;
    cwnd->dupAck = 0;
    cwnd->ssthresh = 4096;
    _toSlowStart(cwnd);
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
    if (cwnd->state.state = FAST_RECOV_RETRANS) {
        _toFastRecov(cwnd);
        return TRUE;
    } else {
        return FALSE;
    }
}

void timeoutCwnd(Cwnd* cwnd) {
    _halveWindow(cwnd);
    _clearDup(cwnd);
    // set to 1 after halve the window
    cwnd->window = 1;
    _toSlowStart(cwnd);
}

size_t getCwnd(Cwnd* cwnd) {
    return cwnd->window;
}