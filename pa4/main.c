#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include "common.h"
#include "ipc.h"
#include "pipes.h"
#include "pa2345.h"
#include "lamport.h"
#include "list.h"

fd_pair** pipes; // pipe matrix
FILE* events_log_file;
FILE* pipes_log_file;
local_id X = 0;
local_id cur_id = 0;
local_id active = 0;
bool mutexl = false;
SortedRequestList request_list = {0};

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

/** Fill header of a message. IMPORTANT: Increments lamport time! */
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

/** Parse message header and possibly decrement number of working processes */
void handle_request(MessageHeader header, int8_t* not_replied_num, int8_t* active_num) {
    switch (header.s_type) {
        case CS_REQUEST:
            if (mutexl) {
                add_to_list(&request_list, (Request) {header.s_local_time, last_sender_id});
            }
            Message* msg2 = calloc(MAX_MESSAGE_LEN, 1);
            init_message_header(msg2, CS_REPLY);
            send(pipes[cur_id], last_sender_id, msg2);
            free(msg2);
            break;
        case CS_RELEASE:
            if (mutexl) {
                remove_from_list_by_pid(&request_list, last_sender_id);
            }
            break;
        case CS_REPLY:
            (*not_replied_num)--;
            break;
        case DONE:
            (*active_num)--;
            break;
        default:
            printf(received_wrong_type_fmt, header.s_type);
            exit(-1);
    }
}

int request_cs(__attribute__((unused)) const void* self) {
    /* Send CS_REQUEST to everyone */
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    init_message_header(msg, CS_REQUEST);
    send_multicast(pipes, msg);
    add_to_list(&request_list, (Request) {get_lamport_time(), cur_id});

    /* Receive CS_REPLY from everyone */
    int8_t not_replied = X;
    while (not_replied || request_list.data[0].pid != cur_id) {
        receive_any(pipes, msg);
        sync_lamport_time(msg->s_header.s_local_time);
        inc_and_get_lamport_time();
        handle_request(msg->s_header, &not_replied, &active);
    }
    free(msg);
    return 0;
}

int release_cs(__attribute__((unused)) const void* self) {
    /* Send CS_RELEASE to everyone */
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    init_message_header(msg, CS_RELEASE);
    send_multicast(pipes, msg);
    remove_from_list_by_pid(&request_list, cur_id);
    free(msg);
    return 0;
}

/** Task of child */
void child_task(void) {
    close_unused_pipes();

    /* Send STARTED to everyone */
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    init_message_header(msg, STARTED);
    sprintf(msg->s_payload, log_started_fmt, get_lamport_time(), cur_id, getpid(), getppid(), 0);
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    send_multicast(pipes, msg);
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);

    /* Receive STARTED from everyone */
    receive_all(pipes, STARTED);
    printf(log_received_all_started_fmt, get_lamport_time(), cur_id);
    fprintf(events_log_file, log_received_all_started_fmt, get_lamport_time(), cur_id);

    /* Work */
    for (int i = 1; i <= cur_id * 5; i++) {
        char buffer[50] = {0};
        sprintf(buffer, log_loop_operation_fmt, cur_id, i, cur_id * 5);
        if (mutexl) {
            request_cs(NULL);
            print(buffer);
            release_cs(NULL);
        } else {
            print(buffer);
        }
    }

    /* Send DONE to everyone */
    init_message_header(msg, DONE);
    sprintf(msg->s_payload, log_done_fmt, get_lamport_time(), cur_id, 0);
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    send_multicast(pipes, msg);
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);

    /* Receive DONE from everyone */
    while (active - 1) {
        receive_any(pipes, msg);
        sync_lamport_time(msg->s_header.s_local_time);
        inc_and_get_lamport_time();
        handle_request(msg->s_header, NULL, &active);
    }
    printf(log_received_all_done_fmt, get_lamport_time(), cur_id);
    fprintf(events_log_file, log_received_all_done_fmt, get_lamport_time(), cur_id);

    free(msg);
    fclose(events_log_file);
    close_pipes();
}

/** Task of parent */
void parent_task(void) {
    close_unused_pipes();

    /* Receive STARTED from everyone */
    receive_all(pipes, STARTED);

    /* Receive DONE from everyone */
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    while (active) {
        receive_any(pipes, msg);
        sync_lamport_time(msg->s_header.s_local_time);
        inc_and_get_lamport_time();
        handle_request(msg->s_header, NULL, &active);
    }

    while (wait(NULL) > 0);

    free(msg);
    fclose(events_log_file);
    close_pipes();
}

int main(int argc, char* argv[]) {
    int mutexl_flag = 0;
    struct option long_options[] = {
            {"mutexl", no_argument, &mutexl_flag, 1},
            {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "p:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                active = X = (local_id) atoi(optarg);
                if (X < 1 || X > 10) {
                    printf("Value must be in range [1..10]!\n");
                    return 1;
                }
                break;
            case 0:
                break;
            case '?':
            default:
                printf("Usage: -p <X> [--mutexl]\n");
                return 1;
        }
    }

    if (argc <= 1 || !X) {
        printf("Usage: -p <X> [--mutexl]\n");
        return 1;
    }

    create_log_files();
    create_pipes();
    fclose(pipes_log_file); // close pipes log to prevent duplicate write
    for (local_id i = 1; i <= X; i++) {
        if (fork() == 0) {
            cur_id = i;
            mutexl = mutexl_flag;
            child_task();
            return 0;
        }
    }
    parent_task();

    return 0;
}
