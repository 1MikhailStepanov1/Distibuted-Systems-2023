#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"
#include "ipc.h"
#include "pipes.h"
#include "pa2345.h"
#include "lamport.h"

fd_pair** pipes; // pipe matrix
FILE* events_log_file;
FILE* pipes_log_file;
local_id X;
local_id cur_id = 0;

extern local_id last_sender_id;

const char* received_wrong_type_fmt = "Got wrong message (type %d)!\n";

/** Create (or open) log files */
void create_log_files(void) {
    events_log_file = fopen(events_log, "a");
    if (events_log_file == NULL) {
        printf("Can't open file\"%s\"!", events_log);
        exit(-1);
    }
    pipes_log_file = fopen(pipes_log, "a");
    if (pipes_log_file == NULL) {
        printf("Can't open file\"%s\"!", pipes_log);
        exit(-1);
    }
}

/** Fill header of a message */
void init_message_header(Message* msg, MessageType type) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_payload_len = 0;
    msg->s_header.s_local_time = inc_and_get_lamport_time();
    msg->s_header.s_type = type;
}

/** Receive messages from all processes */
int receive_all(void* self, MessageType type) {
    Message* msg = malloc(MAX_MESSAGE_LEN);
    for (local_id i = 1; i <= X; i++) {
        if (i != cur_id) {
            memset(msg, 0, MAX_MESSAGE_LEN);
            while (1) {
                int result = receive(((fd_pair**) self)[i], cur_id, msg);
                if (result == -1) {
                    printf("Receive from all failed!");
                    exit(-1);
                } else if (result == 0) {
                    sync_lamport_time(msg->s_header.s_local_time);
                    inc_and_get_lamport_time();
                    break;
                }
            }
            if (msg->s_header.s_type != type) {
                printf(received_wrong_type_fmt, msg->s_header.s_type);
                exit(-1);
            }
        }
    }
    free(msg);
    return 0;
}

/** Task of parent */
void parent_task(void) {
    close_unused_pipes();

    receive_all(pipes, STARTED); // receiving STARTED messages

    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    init_message_header(msg, STOP);
    send_multicast(pipes, msg); // sending STOP messages
    free(msg);

    while (wait(NULL) > 0);

    fclose(events_log_file);
    close_pipes();
}

/** Task of child */
void child_task(void) {
    close_unused_pipes();
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);

    init_message_header(msg, STARTED);
    sprintf(msg->s_payload, log_started_fmt, get_lamport_time(), cur_id, getpid(), getppid(), 0);
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    send_multicast(pipes, msg); // send STARTED to everyone

    receive_all(pipes, STARTED); // receive STARTED from everyone
    printf(log_received_all_started_fmt, get_lamport_time(), cur_id);
    fprintf(events_log_file, log_received_all_started_fmt, get_lamport_time(), cur_id);

    uint8_t running = X;
    while (running) {
        receive_any(pipes, msg);
        sync_lamport_time(msg->s_header.s_local_time);
        inc_and_get_lamport_time();
        if (msg->s_header.s_type == CS_REQUEST) {

        } else if (msg->s_header.s_type == CS_RELEASE) {

            running--;
        } else if (msg->s_header.s_type == CS_REPLY) {
            running--;
        } else if (msg->s_header.s_type == DONE) {
            running--;
        } else {
            printf(received_wrong_type_fmt, msg->s_header.s_type);
            exit(-1);
        }
    }

    free(msg);
    fclose(events_log_file);
    close_pipes();
}

int main(int argc, char* argv[]) {
    if (strcmp(argv[1], "-p") == 0) {
        X = (local_id) atoi(argv[2]);
        if (X < 2 || X > 10) {
            printf("Value must be in range [1..10]!");
            return 1;
        }
        if (argc == X + 3) {
            create_log_files();
            create_pipes();
            fclose(pipes_log_file); // close pipes log to prevent duplicate write
            for (local_id i = 1; i <= X; i++) {
                if (fork() == 0) {
                    cur_id = i;
                    child_task();
                    return 0;
                }
            }
            parent_task();
        } else {
            printf("Not enough balances specified (expected %d, got %d)!\n", X, argc - 3);
            return 1;
        }
    } else {
        printf("Usage: -p <X> <value 1> ... <value X>\n");
        return 1;
    }
    return 0;
}
