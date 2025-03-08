#ifndef TIMER_HEADER
#define TIMER_HEADER
#define tv_nsec tv_usec

#include <sys/time.h>
#include <unistd.h>

//typedef struct timespec TimeStamp;
typedef unsigned int utime_t;

typedef struct timeval TimeStamp;
typedef suseconds_t microseconds_t;
typedef suseconds_t miliseconds_t;
typedef time_t seconds_t;

// TODO: move to globals
extern TimeStamp INIT_TIME;
extern TimeStamp TIMER_NOW;
extern TimeStamp TIMER_NEVER;
extern miliseconds_t TIMER_NOW_MS;
extern miliseconds_t TIMER_NEVER_MS;

void timer_init(int);
void timer_synchronize();
void timer_print(TimeStamp*);
void timer_print_now();
void timer_never(TimeStamp*);
void timer_now(TimeStamp*); // Calls gettimeofday() to retrieve current time
int timer_ready(TimeStamp*);
int timer_nready(TimeStamp*);
int timer_sleep(TimeStamp*);
miliseconds_t timer_to_ms(TimeStamp*);
void timer_add_ms(TimeStamp*, utime_t);
TimeStamp timer_diff(TimeStamp*, TimeStamp*);
unsigned long timer_diff_milisec(TimeStamp*, TimeStamp*);

#endif
