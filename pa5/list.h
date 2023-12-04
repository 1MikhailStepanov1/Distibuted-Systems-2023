#ifndef PA4_LIST_H
#define PA4_LIST_H
#include "ipc.h"

/** Struct which represents request to critical section */
typedef struct Request {
    timestamp_t time;
    local_id pid;
} Request;

/** List which stores Requests sorted by timestamp (maximum of MAX_PROCESS_ID elements).
 * If timestamps are the same, elements are stored in the order they entered the list. */
typedef struct SortedRequestList {
    Request data[MAX_PROCESS_ID];
    int8_t size;
} SortedRequestList;

/** Add request to list */
void add_to_list(SortedRequestList* list, Request request);

/** Remove request with specific pid from list */
void remove_from_list_by_pid(SortedRequestList* list, local_id pid);

#endif
