#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "pipes.h"
#include "ipc.h"

extern FILE* events_log_file;
extern local_id X;
extern local_id cur_id;

local_id last_sender_id;

int send(void* self, local_id dst, const Message* msg) {
    ssize_t message_size = (ssize_t) (sizeof(MessageHeader) + msg->s_header.s_payload_len);
    ssize_t bytes_left = message_size;

    while (bytes_left) {
        ssize_t bytes_written = write(((fd_pair*) self)[dst].fd[1], msg + (message_size - bytes_left), bytes_left);
        if (bytes_written == -1) {
            if (errno == EAGAIN) {
                continue;
            }
            return -1;
        }
        bytes_left -= bytes_written;
    }
    return 0;
}

int receive(void* self, local_id from, Message* msg) {
    ssize_t message_size = (ssize_t) sizeof(MessageHeader);
    ssize_t bytes_left = message_size;
    while (bytes_left) {
        ssize_t bytes_read = read(((fd_pair*) self)[from].fd[0], msg + (message_size - bytes_left), bytes_left);
        if (bytes_read == -1) {
            if (errno == EAGAIN) {
                return 1; // if there are no messages, no need to wait
            }
            return -1;
        } else if (bytes_read == 0) {
            return 1; // if there are no messages, no need to wait
        }
        bytes_left -= bytes_read;
    }

    message_size = msg->s_header.s_payload_len;
    bytes_left = message_size;
    while (bytes_left) {
        ssize_t bytes_read = read(((fd_pair*) self)[from].fd[0], &msg->s_payload + (message_size - bytes_left),
                                  bytes_left);
        if (bytes_read == -1) {
            if (errno == EAGAIN) {
                continue;
            }
            return -1;
        }
        bytes_left -= bytes_read;
    }

    return 0;
}

int send_multicast(void* self, const Message* msg) {
    for (local_id i = 0; i <= X; i++) {
        if (i != cur_id) {
            if (send(((fd_pair**) self)[cur_id], i, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int receive_any(void* self, Message* msg) {
    while (1) {
        for (local_id i = 0; i <= X; i++) {
            if (cur_id != i) {
                int result = receive(((fd_pair**) self)[i], cur_id, msg);
                if (result == -1) {
                    printf("Receive from all failed!");
                    exit(-1);
                } else if (result == 0) {
                    last_sender_id = i;
                    return 0;
                }
            }
        }
    }
}
