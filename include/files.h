#ifndef FILES_HEADER
#define FILES_HEADER

#include "scheduler.h"

#include "fcntl.h"
#include "unistd.h"
#include "aio.h"


int file_open(Task*, Stack64*);
int file_close(int fd);
int await_file_read(Task*, Stack64*);
int await_file_read(Task*, Stack64*);

#endif
