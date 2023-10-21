#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"
#include "ipc.h"
#include "pipes.h"

fd_pair **pipes; // pipe matrix
FILE *events_log_file;
FILE *pipes_log_file;
local_id X;
local_id cur_id = 0;

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

/** Receive messages from all processes */
int receive_all(void *self, Message *msg) {
    for (local_id i = 1; i <= X; i++) {
        if (i != cur_id) {
            memset(msg, 0, MAX_MESSAGE_LEN);
            if (receive(((fd_pair **) self)[i], cur_id, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

void task(bool isChild, uint8_t balance) {
    close_unused_pipes();
    Message *msg = calloc(MAX_MESSAGE_LEN, 1);

    if (isChild) {
        /*
        init_message_header(msg, STARTED);
        sprintf(msg->s_payload, log_started_fmt, cur_id, getpid(), getppid());
        msg->s_header.s_payload_len = strlen(msg->s_payload);
        send_multicast(pipes, msg);

        receive_all(pipes, msg);
        printf(log_received_all_started_fmt, cur_id);
        fprintf(events_log_file, log_received_all_started_fmt, cur_id);
        memset(msg, 0, MAX_MESSAGE_LEN);

        init_message_header(msg, DONE);
        sprintf(msg->s_payload, log_done_fmt, cur_id);
        msg->s_header.s_payload_len = strlen(msg->s_payload);
        send_multicast(pipes, msg);

        receive_all(pipes, msg);
        printf(log_received_all_done_fmt, cur_id);
        fprintf(events_log_file, log_received_all_done_fmt, cur_id);
         */
    } else {
        /* receive_all(pipes, msg);
        receive_all(pipes, msg);
        while (wait(NULL) > 0);
         */
    }
    free(msg);
    fclose(events_log_file);
    close_pipes();
}

int main(int argc, char *argv[]) {
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
                    task(true, (uint8_t) atoi(argv[i + 2]));
                    return 0;
                }
            }
            task(false, 0);
        } else {
            printf("Not enough balances specified (expected %d, got %d)!\n", X, argc - 3);
            return 1;
        }
    } else {
        printf("Usage: -p <X> <value 1> ... <value X>\n");
        return 1;
    }
    //bank_robbery(parent_data);
    //print_history(all);
    return 0;
}
