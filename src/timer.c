#include "globals.h"
#include "timer.h"
#include "stdio.h"
#include "string.h"

#include <time.h>

TimeStamp INIT_TIME;
TimeStamp TIMER_NOW;
TimeStamp TIMER_NEVER = {0, 0};
milliseconds_t INIT_TIME_MS;
milliseconds_t TIMER_NOW_MS;
milliseconds_t TIMER_NEVER_MS = -1;

TimeStamp last_sync, refresh_rate;

void timer_init(int ips) {
    refresh_rate.tv_sec = ips / 1000000;
    refresh_rate.tv_usec = 1000000 / ips;

    timer_now(&INIT_TIME);
    timer_now(&TIMER_NOW);
    timer_now(&last_sync);

    INIT_TIME_MS = timer_to_ms(&TIMER_NOW);
    TIMER_NOW_MS = INIT_TIME_MS;
}

void timer_now(TimeStamp* ts) {
    //clock_gettime(CLOCK_MONOTONIC, ts);
    gettimeofday(ts, NULL);
}

void timer_never(TimeStamp* ts) {
    static TimeStamp never;
    never.tv_sec = -1;
    never.tv_usec = -1;
    memcpy(ts, &never, sizeof(TimeStamp));
}

void timer_add_ms(TimeStamp *ts, microseconds_t ms) {
    seconds_t sec = ts->tv_sec;
    microseconds_t usec = ts->tv_usec;

    sec += ms / 1000;
    usec += ms * 1000;

    int carry = usec < ts->tv_usec;
    ts->tv_sec = sec + carry;
    ts->tv_usec = usec;
}

TimeStamp timer_diff(TimeStamp* a, TimeStamp* b) {
    TimeStamp time_diff;

    seconds_t asec = a->tv_sec;
    seconds_t bsec = b->tv_sec;
    microseconds_t ausec = a->tv_usec;
    microseconds_t busec = b->tv_usec;

    time_diff.tv_sec = (bsec < asec) * (asec - bsec);
    time_diff.tv_usec = (0 <= asec - bsec) * (busec < ausec) * (ausec - busec);

    return time_diff;
}

milliseconds_t timer_diff_millisec(TimeStamp* a, TimeStamp* b) {
    milliseconds_t diff = 0;

    seconds_t asec = a->tv_sec;
    seconds_t bsec = b->tv_sec;
    microseconds_t ausec = a->tv_usec;
    microseconds_t busec = b->tv_usec;

    diff += (bsec < asec) * (asec - bsec) * 1000;
    diff += (0 <= asec - bsec) * (busec < ausec) * (ausec - busec) / 1000;

    return diff;
}

int timer_nready(TimeStamp* ts) {
    return !timer_ready(ts);
}

int timer_ready(TimeStamp* ts) {
    if (ts->tv_sec < 0 || ts->tv_usec < 0) return 0;

    seconds_t asec = TIMER_NOW.tv_sec;
    seconds_t bsec = ts->tv_sec;
    microseconds_t ausec = TIMER_NOW.tv_usec;
    microseconds_t busec = ts->tv_usec;

    seconds_t delta_sec = (bsec < asec) * (asec - bsec);
    microseconds_t delta_usec = (0 <= asec - bsec) * (busec < ausec) * (ausec - busec);

    return delta_sec > 0 || delta_usec > 0;
}

int timer_sleep(TimeStamp* t) {
    return usleep(6000000 * t->tv_sec + t->tv_usec);
}

milliseconds_t timer_to_ms(TimeStamp *ts) {
    return ts->tv_sec * 1000 + ts->tv_usec / 1000;
}

void timer_print(TimeStamp* t) {
    static long unsigned int hours, minutes, seconds, useconds;
    hours = t->tv_sec / (60*60) % (24);
    minutes = t->tv_sec / (60) % (60);
    seconds = t->tv_sec % 60;
    useconds = t->tv_usec;// / 10000;
    _log_debug("<%02lu:%02lu:%02lu.%lu>", hours, minutes, seconds, useconds);
}

void timer_print_now() {
    timer_print(&TIMER_NOW);
}

void timer_synchronize() {
    static TimeStamp delta, sleepts;
    timer_now(&TIMER_NOW);
    TIMER_NOW_MS = timer_to_ms(&TIMER_NOW);

    delta = timer_diff(&TIMER_NOW, &last_sync);
    sleepts = timer_diff(&refresh_rate, &delta);

    timer_sleep(&sleepts);

    timer_now(&last_sync);
}

