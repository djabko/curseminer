#include "globals.h"
#include "files.h"

#include <errno.h>
#include <stdio.h>

int file_open(Task* task, Stack64* stack) {
    char* pathname = (char*) st_pop(stack);
    int nbytes = st_pop(stack);
    int flags = st_pop(stack);

    int fd = open(pathname, flags);

    char* buf = pathname;

    struct aiocb* cb = malloc(sizeof (struct aiocb));
    cb->aio_fildes = fd;
    cb->aio_offset = 0;
    cb->aio_buf = buf;
    cb->aio_nbytes = nbytes - 1;
    cb->aio_reqprio = 0;
    cb->aio_sigevent.sigev_notify = SIGEV_NONE;
    cb->aio_lio_opcode = LIO_NOP;
    aio_read(cb);

    Stack64* new_stack = st_init(1);
    st_push(new_stack, (uint64_t) cb);

    log_debug_nl();
    return 0;
}

int file_read(Task* task, Stack64* stack) {
    struct aiocb* cb = NULL;

    // If stack->top is 0, start read operation
    if (st_peek(stack) == 0) {
        st_pop(stack);
        cb = (struct aiocb*) st_peek(stack);
        aio_read(cb);

    } else {
        cb = (struct aiocb*) st_peek(stack);
    }

    ssize_t err_val = aio_error(cb);
    switch (err_val) {
        // Awaiting read operation
        case EINPROGRESS:
            return 0;

        // Read operation finished
        case 0:

            log_debug("'''%p'''", cb->aio_buf);

            size_t status = aio_return(cb);

            // File has not yet been read in full
            if (status == cb->aio_nbytes) {}
                //schedule(RQ_LOW, NULL, await_file_read, stack);

            // EOF encountered, file has been read in full
            else if (0 <= status && status < cb->aio_nbytes) {
                log_debug("EOF encountered, file read in full");
                tk_kill(task);
            }

            // Error case
            else
                log_debug("Error reading file...");
        // Error case
        default:
            log_debug("File reading error...");
            tk_kill(task);
            free(cb);
    }

    return 0;
}

int await_file_read(Task* task, Stack64* stack) {
    struct aiocb* cb = (struct aiocb*) *(stack->head);

    switch (aio_error(cb)) {
        case 0:
            log_debug("File contents: '%p'", cb->aio_buf);
            file_close(cb->aio_fildes);
            free((void*) cb->aio_buf);
            free(cb);
            tk_kill(task);
            return 0;

        case EINPROGRESS:
            log_debug("In progress...");
            return 1;

        case ECANCELED:
            log_debug("Cancelled!!");
            return -1;

        default:
            log_debug("ERROR occured while reading file.'");
            exit(1);
    }
}

int file_close(int fd) {
    return close(fd);
}

