#ifndef SIGNAL_HEADER
#define SIGNAL_HEADER

enum {

    add_ticket,
    rm_ticket,
    update_ticket

} SIGNALS;

typedef struct {
    int type, param1;
    uint64_t param2, param3, param4;
} Signal;

#endif
