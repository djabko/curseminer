#include "files.h"
#include "run_queue.h"

#include <errno.h>
#include <stdio.h>

int file_open(const char *pathname, int flags) {
    return open(pathname, flags);
}

int file_read(Task* task, Stack64* stack) {
    struct aiocb* cb = (struct aiocb*) *(stack->top);

    //st_print(stack);

    //printf("Buf: %s\n", cb->aio_buf);

    int error, finished;
    error = aio_read(cb) == -1;
    finished = !(aio_error(cb) == EINPROGRESS);


    if (error) {
        printf("File: 'ERROR OCCURED'\n");
        kill_task(task);
        free(cb->aio_buf);
        free(cb);
    } else if (finished) {
        printf("File: '%s'\n", cb->aio_buf);
        kill_task(task);
        free(cb->aio_buf);
        free(cb);
    }
}

int file_close(int fd) {
    return close(fd);
}

