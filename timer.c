#include "timer.h"
#include "stdio.h"
#include "string.h"

#include <time.h>

TimeStamp INIT_TIME;
TimeStamp last_sync, refresh_rate;

void timer_init(int ips) {
    refresh_rate.tv_sec = ips / 1000000;
    refresh_rate.tv_nsec = 1000000 / ips;

    timer_now(&INIT_TIME);
    timer_now(&last_sync);
}

void timer_now(TimeStamp* ts) {
    //clock_gettime(CLOCK_MONOTONIC, ts);
    gettimeofday(ts, NULL);
}

void timer_never(TimeStamp* ts) {
    static TimeStamp never;
    never.tv_sec = -1;
    never.tv_nsec = -1;
    memcpy(ts, &never, sizeof(TimeStamp));
}

TimeStamp timer_diff(TimeStamp* a, TimeStamp* b) {
    TimeStamp time_diff;

    seconds_t asec = a->tv_sec;
    seconds_t bsec = b->tv_sec;
    microseconds_t ausec = a->tv_nsec;
    microseconds_t busec = b->tv_nsec;

    time_diff.tv_sec = (bsec < asec) * (asec - bsec);
    time_diff.tv_nsec = (0 <= asec - bsec) * (busec < ausec) * (ausec - busec);

    return time_diff;
}

int timer_ready(TimeStamp* ts) {
    return !timer_nready(ts);
}

int timer_nready(TimeStamp* ts) {
    if (ts->tv_sec < 0 || ts->tv_nsec < 0) return 0;

    TimeStamp now;
    timer_now(&now);

    seconds_t asec = now.tv_sec;
    seconds_t bsec = ts->tv_sec;
    microseconds_t ausec = now.tv_nsec;
    microseconds_t busec = ts->tv_nsec;

    seconds_t delta_sec = (bsec < asec) * (asec - bsec);
    microseconds_t delta_usec = (0 <= asec - bsec) * (busec < ausec) * (ausec - busec);

    return delta_sec > 0 || delta_usec > 0;
}

int timer_sleep(TimeStamp* t) {
    return usleep(6000000 * t->tv_sec + t->tv_nsec);
}

void timer_print(TimeStamp* t) {
    static long unsigned int years, months, days, hours, minutes, seconds, useconds;
    hours = t->tv_sec / (60*60) % (24);
    minutes = t->tv_sec / (60) % (60);
    seconds = t->tv_sec % 60;
    useconds = t->tv_nsec;// / 10000;
    printf("<%02lu:%02lu:%02lu.%lu>", hours, minutes, seconds, useconds);
}

void timer_print_now() {
    TimeStamp now;
    timer_now(&now);
    timer_print(&now);
}

void timer_synchronize() {
    static TimeStamp now, delta, sleepts;
    timer_now(&now);

    delta = timer_diff(&now, &last_sync);
    sleepts = timer_diff(&refresh_rate, &delta);

    timer_sleep(&sleepts);

    timer_now(&last_sync);
}

