#ifndef FILES_HEADER
#define FILES_HEADER

#include "run_queue.h"

#include "fcntl.h"
#include "unistd.h"
#include "aio.h"


int file_open(const char *pathname, int flags);
int file_close(int fd);
int file_read(Task*, Stack64*);
int file_write(Task*, Stack64*);

#endif
