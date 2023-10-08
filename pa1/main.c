#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"
#include "ipc.h"
#include "pa1.h"

const char* log_pipe_open_fmt = "Pipe (%d; %d) created. read_fd=%d, write_fd=%d\n";

/** Struct which represents descriptors of pipe */
typedef struct fd_pair {
    int fd[2];
} fd_pair;

fd_pair** pipes; // pipe matrix
local_id X;
local_id cur_id = 0;

FILE* events_log_file;
FILE* pipes_log_file;

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

int send(void* self, local_id dst, const Message* msg) {
    long bytes_written = write(((fd_pair*) self)[dst].fd[1], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return bytes_written != (sizeof(MessageHeader) + msg->s_header.s_payload_len);
}

int send_multicast(void* self, const Message* msg) {
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);
    for (local_id i = 0; i <= X; i++) {
        if (i != cur_id) {
            if (send(((fd_pair**) self)[cur_id], i, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int receive(void* self, local_id from, Message* msg) {
    long bytes_read = read(((fd_pair*) self)[from].fd[0], msg, sizeof(MessageHeader));
    bytes_read += read(((fd_pair*) self)[from].fd[0], &msg->s_payload, msg->s_header.s_payload_len);
    return bytes_read <= 0;
}

/** Receive messages from all processes */
int receive_all(void* self, Message* msg) {
    for (local_id i = 1; i <= X; i++) {
        if (i != cur_id) {
            memset(msg, 0, MAX_MESSAGE_LEN);
            if (receive(((fd_pair**) self)[i], cur_id, msg) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

/** Allocates space for pipe-matrix */
void create_pipes(void) {
    pipes = malloc(sizeof(fd_pair*) * (X + 1));
    for (local_id i = 0; i <= X; i++) {
        pipes[i] = malloc(sizeof(fd_pair) * (X + 1));
        for (local_id j = 0; j <= X; j++) {
            if (i != j) {
                pipe(pipes[i][j].fd);
                fprintf(pipes_log_file, log_pipe_open_fmt, i, j, pipes[i][j].fd[0], pipes[i][j].fd[1]);
            }
        }
    }
}

/** Close descriptors of pipes that aren't used by the process */
void close_unused_pipes(void) {
    for (local_id i = 0; i <= X; i++) {
        for (local_id j = 0; j <= X; j++) {
            if (i != cur_id && j != cur_id && i != j) {
                close(pipes[i][j].fd[0]);
                close(pipes[i][j].fd[1]);
            }
        }
        if (i != cur_id) {
            close(pipes[cur_id][i].fd[0]);
            close(pipes[i][cur_id].fd[1]);
        }
    }
}

/** Close all pipes after task is done and free space */
void close_pipes(void) {
    for (local_id i = 0; i <= X; i++) {
        if (i != cur_id) {
            close(pipes[cur_id][i].fd[1]);
            close(pipes[i][cur_id].fd[0]);
        }
        free(pipes[i]);
    }
    free(pipes);
}

/** Fill header of a message */
void init_message_header(Message* msg, MessageType type) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_local_time = 0;
    msg->s_header.s_type = type;
}

/** Execute parent or child task */
void task(bool isChild) {
    close_unused_pipes();
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);

    if (isChild) {
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
    } else {
        receive_all(pipes, msg);
        receive_all(pipes, msg);
        while (wait(NULL) > 0);
    }
    free(msg);
    fclose(events_log_file);
    close_pipes();
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        X = (local_id) atoi(argv[2]);
        if (X > 0 && X < 11) {
            create_log_files();
            create_pipes();
            fclose(pipes_log_file); // close pipes log to prevent duplicate write
            for (local_id i = 1; i <= X; i++) {
                if (fork() == 0) {
                    cur_id = i;
                    task(true);
                    return 0;
                }
            }
            task(false);
        } else {
            printf("Value must be in range [1..10]!");
            return 1;
        }
    } else {
        printf("Usage: -p <value>");
        return 1;
    }
    return 0;
}
