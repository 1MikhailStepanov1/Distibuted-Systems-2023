#include "list.h"

void add_to_list(SortedRequestList* list, Request request) {
    int8_t i = 0;
    for (; i < list->size && list->data[i].time < request.time; i++);
    for (; i < list->size && list->data[i].time == request.time && list->data[i].pid < request.pid; i++);
    for (int8_t j = list->size++; j > i; j--) {
        list->data[j] = list->data[j - 1];
    }
    list->data[i] = request;
}

void remove_from_list_by_pid(SortedRequestList* list, local_id pid) {
    int8_t i = 0;
    for (; i < list->size && list->data[i].pid != pid; i++);
    for (int8_t j = i; j < list->size - 1; j++) {
        list->data[j] = list->data[j + 1];
    }
    list->size--;
}
