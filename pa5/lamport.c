#include "lamport.h"

timestamp_t time = 0;

timestamp_t inc_and_get_lamport_time(void) {
    return ++time;
}

void sync_lamport_time(timestamp_t value) {
    if (value > time) {
        time = value;
    }
}

timestamp_t get_lamport_time(void) {
    return time;
}
