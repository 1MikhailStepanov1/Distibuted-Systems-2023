#ifndef PA3_LAMPORT_H
#define PA3_LAMPORT_H

#include "banking.h"

/** Increments lamport time and returns it */
timestamp_t inc_and_get_lamport_time(void);

/** Synchronizes lamport time: if value > time, then time = value */
void sync_lamport_time(timestamp_t value);

#endif
