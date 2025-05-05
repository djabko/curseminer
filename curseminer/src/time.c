#include <stdio.h>
#include <string.h>

#include "curseminer/globals.h"
#include "curseminer/time.h"
#include "curseminer/arch.h"

TimeStamp INIT_TIME;
TimeStamp TIMER_NOW;
TimeStamp TIMER_NEVER = {0, 0};
milliseconds_t INIT_TIME_MS;
milliseconds_t TIMER_NOW_MS;
milliseconds_t TIMER_NEVER_MS = -1;

TimeStamp last_sync, refresh_rate;

void time_init(int ips) {
    refresh_rate.sec = ips / 1000000;
    refresh_rate.usec = 1000000 / ips;

    time_now(&INIT_TIME);
    time_now(&TIMER_NOW);
    time_now(&last_sync);

    INIT_TIME_MS = time_to_ms(&TIMER_NOW);
    TIMER_NOW_MS = INIT_TIME_MS;
}

void time_now(TimeStamp* ts) {
    arch_get_time_monotonic(ts);
}

void time_never(TimeStamp* ts) {
    static TimeStamp never;
    never.sec = -1;
    never.usec = -1;
    memcpy(ts, &never, sizeof(TimeStamp));
}

void time_add_ms(TimeStamp *ts, microseconds_t ms) {
    seconds_t sec = ts->sec;
    microseconds_t usec = ts->usec;

    sec += ms / 1000;
    usec += ms * 1000;

    int carry = usec < ts->usec;
    ts->sec = sec + carry;
    ts->usec = usec;
}

TimeStamp time_diff(TimeStamp* a, TimeStamp* b) {
    TimeStamp time_diff;

    seconds_t asec = a->sec;
    seconds_t bsec = b->sec;
    microseconds_t ausec = a->usec;
    microseconds_t busec = b->usec;

    time_diff.sec = (bsec < asec) * (asec - bsec);
    time_diff.usec = (0 <= asec - bsec) * (busec < ausec) * (ausec - busec);

    return time_diff;
}

milliseconds_t time_diff_millisec(TimeStamp* a, TimeStamp* b) {
    milliseconds_t diff = 0;

    seconds_t asec = a->sec;
    seconds_t bsec = b->sec;
    microseconds_t ausec = a->usec;
    microseconds_t busec = b->usec;

    diff += (bsec < asec) * (asec - bsec) * 1000;
    diff += (0 <= asec - bsec) * (busec < ausec) * (ausec - busec) / 1000;

    return diff;
}

int time_nready(TimeStamp* ts) {
    return !time_ready(ts);
}

int time_ready(TimeStamp* ts) {
    if (ts->sec < 0 || ts->usec < 0) return 0;

    seconds_t asec = TIMER_NOW.sec;
    seconds_t bsec = ts->sec;
    microseconds_t ausec = TIMER_NOW.usec;
    microseconds_t busec = ts->usec;

    seconds_t delta_sec = (bsec < asec) * (asec - bsec);
    microseconds_t delta_usec = (0 <= asec - bsec) * (busec < ausec) * (ausec - busec);

    return delta_sec > 0 || delta_usec > 0;
}

int time_sleep(TimeStamp* ts) {
    return arch_sleep(ts);
}

milliseconds_t time_to_ms(TimeStamp *ts) {
    return ts->sec * 1000 + ts->usec / 1000;
}

void time_print(TimeStamp* t) {
    static long unsigned int hours, minutes, seconds, useconds;
    hours = t->sec / (60*60) % (24);
    minutes = t->sec / (60) % (60);
    seconds = t->sec % 60;
    useconds = t->usec;// / 10000;
    _log_debug("<%02lu:%02lu:%02lu.%lu>", hours, minutes, seconds, useconds);
}

void time_print_now() {
    time_print(&TIMER_NOW);
}

void time_synchronize() {
    static TimeStamp delta, sleepts;

    time_now(&TIMER_NOW);

    TIMER_NOW_MS = time_to_ms(&TIMER_NOW);
    delta = time_diff(&TIMER_NOW, &last_sync);
    sleepts = time_diff(&refresh_rate, &delta);

    time_sleep(&sleepts);
    time_now(&last_sync);
}

