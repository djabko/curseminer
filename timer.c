#include "timer.h"
#include "stdio.h"
#include "string.h"

TimeStamp INIT_TIME;
TimeStamp last_sync, refresh_rate;

void timer_init(int ips) {
    refresh_rate.tv_sec = ips / 1000000;
    refresh_rate.tv_usec = 1000000 / ips;

    gettimeofday(&INIT_TIME, NULL);
    gettimeofday(&last_sync, NULL);
}

TimeStamp timer_now() {
    TimeStamp now;
    gettimeofday(&now, NULL);
    return now;
}

void timer_never(TimeStamp* ts) {
    static TimeStamp never;
    never.tv_sec = -1;
    never.tv_usec = -1;
    memcpy(ts, &never, sizeof(TimeStamp));
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

int timer_ready(TimeStamp* ts) {
    return !timer_nready(ts);
}

int timer_nready(TimeStamp* ts) {
    if (ts->tv_sec < 0 || ts->tv_usec < 0) return 0;

    TimeStamp now;
    gettimeofday(&now, NULL);

    seconds_t asec = now.tv_sec;
    seconds_t bsec = ts->tv_sec;
    microseconds_t ausec = now.tv_usec;
    microseconds_t busec = ts->tv_usec;

    seconds_t delta_sec = (bsec < asec) * (asec - bsec);
    microseconds_t delta_usec = (0 <= asec - bsec) * (busec < ausec) * (ausec - busec);

    return delta_sec > 0 || delta_usec > 0;
}

int timer_sleep(TimeStamp* t) {
    return usleep(6000000 * t->tv_sec + t->tv_usec);
}

void timer_print(TimeStamp* t) {
    static long unsigned int years, months, days, hours, minutes, seconds, useconds;
    hours = t->tv_sec / (60*60) % (24);
    minutes = t->tv_sec / (60) % (60);
    seconds = t->tv_sec % 60;
    useconds = t->tv_usec / 10000;
    printf("<%02lu:%02lu:%02lu.%lu>", hours, minutes, seconds, useconds);
}

void timer_print_now() {
    TimeStamp now;
    gettimeofday(&now, NULL);
    timer_print(&now);
}

void timer_synchronize() {
    static TimeStamp now, delta, sleepts;
    gettimeofday(&now, NULL);

    delta = timer_diff(&now, &last_sync);
    sleepts = timer_diff(&refresh_rate, &delta);

    timer_sleep(&sleepts);
    
    last_sync = now;
}

