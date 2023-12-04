#include "stack.h"

void push_to_stack(PidStack* stack, local_id pid) {
    stack->data[stack->size++] = pid;
}

local_id pop_from_stack(PidStack* stack) {
    return stack->data[--stack->size];
}
