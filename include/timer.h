#ifndef TIMER_HEADER
#define TIMER_HEADER

#include <sys/time.h>
#include <unistd.h>

typedef struct timeval TimeStamp;
typedef suseconds_t microseconds_t;
typedef time_t seconds_t;

extern TimeStamp INIT_TIME;

void timer_init(int);
void timer_synchronize();
void timer_print(TimeStamp*);
void timer_print_now();
void timer_never(TimeStamp*);
int timer_ready(TimeStamp*);
int timer_nready(TimeStamp*);
int timer_sleep(TimeStamp*);
TimeStamp timer_diff(TimeStamp*, TimeStamp*);
TimeStamp timer_now();

#endif
