#include <stdio.h>
#include <unistd.h>
#include "pipes.h"
#include "ipc.h"

extern fd_pair **pipes;
extern FILE *events_log_file;
extern FILE *pipes_log_file;
extern local_id X;
extern local_id cur_id;

int send(void *self, local_id dst, const Message *msg) {
    long bytes_written = write(((fd_pair *) self)[dst].fd[1], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return bytes_written != (sizeof(MessageHeader) + msg->s_header.s_payload_len);
}

int receive(void *self, local_id from, Message *msg) {
    long bytes_read = read(((fd_pair *) self)[from].fd[0], msg, sizeof(MessageHeader));
    bytes_read += read(((fd_pair *) self)[from].fd[0], &msg->s_payload, msg->s_header.s_payload_len);
    return bytes_read <= 0;
}

int send_multicast(void *self, const Message *msg) {
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);
    for (local_id i = 0; i <= X; i++) {
        if (i != cur_id) {
            if (send(((fd_pair **) self)[cur_id], i, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}
