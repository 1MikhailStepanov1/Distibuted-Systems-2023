#ifndef PA5_STACK_H
#define PA5_STACK_H
#include "ipc.h"

/** Stack which stores PIDs of delayed CS_REPLYs (maximum of MAX_PROCESS_ID elements). */
typedef struct PidStack {
    local_id data[MAX_PROCESS_ID];
    int8_t size;
} PidStack;

/** Push pid to stack */
void push_to_stack(PidStack* stack, local_id pid);

/** Pop pid from stack */
local_id pop_from_stack(PidStack* stack);

#endif
