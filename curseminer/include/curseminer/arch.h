#ifndef H_CURSEMINER_ARCH
#define H_CURSEMINER_ARCH

#include "curseminer/time.h"

int arch_sleep(TimeStamp*);
void arch_get_time_monotonic(TimeStamp*);

#endif
