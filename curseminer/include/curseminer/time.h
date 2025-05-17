#ifndef TIMER_HEADER
#define TIMER_HEADER

#include <inttypes.h>
#include <time.h>

typedef uint32_t nanoseconds_t;
typedef uint32_t microseconds_t;
typedef uint32_t milliseconds_t;
typedef time_t seconds_t;

typedef struct TimeStamp {
    nanoseconds_t usec;
    seconds_t sec;
} TimeStamp;

// TODO: move to globals
extern TimeStamp INIT_TIME;
extern TimeStamp TIMER_NOW;
extern TimeStamp TIMER_NEVER;
extern milliseconds_t INIT_TIME_MS;
extern milliseconds_t TIMER_NOW_MS;
extern milliseconds_t TIMER_NEVER_MS;

void time_init(int);
void time_synchronize();

void time_now(TimeStamp*);
void time_never(TimeStamp*);

int time_ready(TimeStamp*);
int time_nready(TimeStamp*);
int time_sleep(TimeStamp*);

void time_print(TimeStamp*);
void time_print_now();

milliseconds_t time_to_ms(TimeStamp*);
TimeStamp time_diff(TimeStamp*, TimeStamp*);
void time_add_ms(TimeStamp*, milliseconds_t);
milliseconds_t time_diff_millisec(TimeStamp*, TimeStamp*);

#endif
