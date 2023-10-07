#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

int X;
int cur_pid = 0;
FILE* events_log_file;
FILE* pipes_log_file;
const char* const log_pipe_open_fmt = "Pipe (%d; %d) created. read_fd=%d, write_fd=%d\n";
const char* const log_pipe_closed_fmt = "Cur_pid: %d. Pipe (%d; %d) closed. read_fd=%d, write_fd=%d\n";

typedef struct fd_pair {
    int fd[2];
} fd_pair;

void create_log_files() {
    events_log_file = fopen(events_log, "a");
    if (events_log_file == NULL) printf("Can't open %s.", events_log);

    pipes_log_file = fopen(pipes_log, "a");
    if (pipes_log_file == NULL) printf("Can't open %s.", pipes_log);
}

int send(void* self, local_id dst, const Message* msg) {
    write(((fd_pair*) self)[dst].fd[1], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return 0;
}

int send_multicast(void* self, const Message* msg) {
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);
    for (int i = 0; i <= X; i++) {
        if (i != cur_pid) {
            send(((fd_pair**) self)[cur_pid], i, msg);
        }
    }
    return 0;
}

int receive(void* self, local_id from, Message* msg) {
    read(((fd_pair*) self)[from].fd[0], msg, sizeof(MessageHeader));
    read(((fd_pair*) self)[from].fd[0], &msg->s_payload, msg->s_header.s_payload_len);
    return 0;
}

int receive_all(void* self, Message* msg){
    for (int i = 1; i <= X; i++){
        if (i != cur_pid){
            receive(((fd_pair**) self)[cur_pid], i, msg);
        }
    }
    return 0;
}

fd_pair** create_pipes(int N, fd_pair** debug_array) {
    fd_pair** pipes = malloc(sizeof(fd_pair*) * N);
    for (int i = 0; i < N; i++) {
        pipes[i] = malloc(sizeof(fd_pair) * N);
        for (int j = 0; j < N; j++) {
            if (i != j) {
                pipe(pipes[i][j].fd);
                debug_array[i][j] = (fd_pair) {{1, 1}};
                fprintf(pipes_log_file, log_pipe_open_fmt, i, j, pipes[i][j].fd[0], pipes[i][j].fd[1]);
            }
        }
    }
    return pipes;
}

void close_unused_pipes(fd_pair** pipes, fd_pair** debug_array, int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i != cur_pid && j != cur_pid && i != j) {
                close(pipes[i][j].fd[0]);
                debug_array[i][j].fd[0] = 0;
                close(pipes[i][j].fd[1]);
                debug_array[i][j].fd[1] = 0;
            }
        }
        if (i != cur_pid) {
            close(pipes[cur_pid][i].fd[0]);
            debug_array[cur_pid][i].fd[0] = 0;
            close(pipes[i][cur_pid].fd[1]);
            debug_array[i][cur_pid].fd[1] = 0;
        }
    }
}

void close_pipes(fd_pair** pipes, int N){
    for (int i = 0; i < N; i++) {
        if (i != cur_pid){
            close(pipes[cur_pid][i].fd[1]);
            close(pipes[i][cur_pid].fd[0]);
        }
        free(pipes[i]);
    }
    free(pipes);
}

void init_message_header(Message* msg, MessageType type) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_local_time = 0;
    msg->s_header.s_type = type;
}

void task(fd_pair** pipes, fd_pair** debug_array, bool isChild) {
    close_unused_pipes(pipes, debug_array, X+1);
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    fclose(pipes_log_file);

    if (isChild){
        init_message_header(msg, STARTED);
        sprintf(msg->s_payload, log_started_fmt, cur_pid, getpid(), getppid());
        msg->s_header.s_payload_len = strlen(msg->s_payload);

        send_multicast(pipes, msg);
        memset(msg, 0, MAX_MESSAGE_LEN);

        receive_all(pipes, msg);
        printf(log_received_all_started_fmt, cur_pid);
        fprintf(events_log_file, log_received_all_started_fmt, cur_pid);
        memset(msg, 0, MAX_MESSAGE_LEN);

        init_message_header(msg, DONE);
        sprintf(msg->s_payload, log_done_fmt, cur_pid);
        msg->s_header.s_payload_len = strlen(msg->s_payload);

        send_multicast(pipes, msg);
        memset(msg, 0, MAX_MESSAGE_LEN);

        receive_all(pipes, msg);
        printf(log_received_all_done_fmt, cur_pid);
        fprintf(events_log_file, log_received_all_done_fmt, cur_pid);
    } else {
        receive_all(pipes, msg);
        memset(msg, 0, MAX_MESSAGE_LEN);
        receive_all(pipes, msg);
        int status;
        while (wait(&status) > 0) {printf("%d\n", status);};
    }
    free(msg);
    fclose(events_log_file);
    close_pipes(pipes, X+1);
}


int main(int argc, char** argv) {

    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        X = atoi(argv[2]);
        if (X > 0 && X < 11) {
            create_log_files();
            fd_pair** debug_array = malloc(sizeof(fd_pair*) * (X+1));
            for (int i = 0; i < X+1; i++){
                debug_array[i] = malloc(sizeof(fd_pair) * (X+1));
            }
            
            fd_pair** pipes = create_pipes(X + 1, debug_array);

            for (int i = 1; i <= X; i++) {
                if (fork() == 0) {
                    cur_pid = i;
                    task(pipes, debug_array, true);
                    return 0;
                }
            }
            task(pipes, debug_array, false);
        } else {
            printf("Value can be in range [1..10].");
            return 1;
        }
    } else {
        printf("Usage: -p <value>");
        return 1;
    }

    return 0;
}
