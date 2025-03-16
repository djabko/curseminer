#ifndef TIMER_HEADER
#define TIMER_HEADER

#include <sys/time.h>
#include <unistd.h>

typedef struct timeval TimeStamp;
typedef suseconds_t microseconds_t;
typedef suseconds_t milliseconds_t;
typedef time_t seconds_t;

// TODO: move to globals
extern TimeStamp INIT_TIME;
extern TimeStamp TIMER_NOW;
extern TimeStamp TIMER_NEVER;
extern milliseconds_t INIT_TIME_MS;
extern milliseconds_t TIMER_NOW_MS;
extern milliseconds_t TIMER_NEVER_MS;

void timer_init(int);
void timer_synchronize();

void timer_now(TimeStamp*);
void timer_never(TimeStamp*);

int timer_ready(TimeStamp*);
int timer_nready(TimeStamp*);
int timer_sleep(TimeStamp*);

void timer_print(TimeStamp*);
void timer_print_now();

milliseconds_t timer_to_ms(TimeStamp*);
TimeStamp timer_diff(TimeStamp*, TimeStamp*);
void timer_add_ms(TimeStamp*, milliseconds_t);
milliseconds_t timer_diff_millisec(TimeStamp*, TimeStamp*);

#endif
