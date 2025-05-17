#include "curseminer/arch.h"


#ifdef __linux__
#include <sys/time.h>
#include <unistd.h>

int arch_sleep(TimeStamp *ts) {
    uint64_t s = ts->sec;
    uint64_t us = 1000000 * s + ts->usec;

    return usleep(us);
}

void arch_get_time_monotonic(TimeStamp *ts) {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);

    seconds_t sec = spec.tv_sec;
    microseconds_t usec = spec.tv_nsec / 1000;

    ts->sec = sec;
    ts->usec = usec;
}


#elifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

// Rounds down to the neareset millisecond
int arch_sleep(TimeStamp *ts) {
    uint64_t s = ts->sec;
    uint64_t us = ts->usec;
    uint64_t ms = 1000 * s + us / 1000;
    int ticks = ms / portTICK_PERIOD_MS;

    if (ticks == 0) ticks = 1;

    vTaskDelay(ticks);

    return 0;
}

void arch_get_time_monotonic(TimeStamp *ts) {
    int64_t us = esp_timer_get_time();

    seconds_t sec = us / 1000000;
    microseconds_t usec = us % 1000000;

    ts->sec = sec;
    ts->usec = usec;
}


#else
#error "Unsupported platform"
#endif
