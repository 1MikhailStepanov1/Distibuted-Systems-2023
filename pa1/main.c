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
int cur_pid;
FILE* events_log_file;
FILE* pipes_log_file;
const char* const log_pipe_open_fmt = "Pipe (%d; %d) created. read_fd=%d, write_fd=%d\n";

typedef struct fd_pair {
    int fd[2];
} fd_pair;

int send(void* self, local_id dst, const Message* msg) {
    write(((fd_pair*) self)[dst].fd[1], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
    return 0;
}

int send_multicast(void* self, const Message* msg) {
    for (int i = 0; i <= X; i++) {
        if (i != cur_pid) {
            send(((fd_pair**) self)[cur_pid], i, msg);
        }
    }
    return 0;
}

int receive(void* self, local_id from, Message* msg) {
    return 0;
}

void create_log_files() {
    events_log_file = fopen(events_log, "a");
    if (events_log_file == NULL) printf("Can't open %s.", events_log);

    pipes_log_file = fopen(pipes_log, "a");
    if (pipes_log_file == NULL) printf("Can't open %s.", pipes_log);
}

fd_pair** create_pipes(int N) {
    fd_pair** pipes = malloc(sizeof(fd_pair*) * N);
    for (int i = 0; i < N; i++) {
        pipes[i] = malloc(sizeof(fd_pair) * N);
        for (int j = 0; j < N; j++) {
            if (i != j) {
                pipe(pipes[i][j].fd);
                fprintf(pipes_log_file, log_pipe_open_fmt, i, j, pipes[i][j].fd[0], pipes[i][j].fd[1]);
            }
        }
    }
    return pipes;
}

void close_unused_pipes(fd_pair** pipes, int pid, int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i != pid && j != pid && i != j) {
                close(pipes[i][j].fd[0]);
                close(pipes[i][j].fd[1]);
            }
        }
        if (i != pid) {
            close(pipes[pid][i].fd[0]);
            close(pipes[i][pid].fd[1]);
        }
    }
}

Message* create_message(MessageType type) {
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_local_time = 0;
    msg->s_header.s_type = type;
    return msg;
}

void child_task(fd_pair** pipes, int pid, int N) {
    close_unused_pipes(pipes, pid, N);

    Message* msg = create_message(STARTED);
    sprintf(msg->s_payload, log_started_fmt, cur_pid, getpid(), getppid());
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    printf("%s", msg->s_payload);
    fprintf(events_log_file, "%s", msg->s_payload);
    send_multicast(pipes, msg);


    free(msg);
    fclose(events_log_file);
}

void root_task(fd_pair** pipes, int X) {

    while (wait(NULL) > 0);
    fclose(events_log_file);
}

int main(int argc, char** argv) {

    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        X = atoi(argv[2]);
        if (X > 0 && X < 11) {
            create_log_files();

            fd_pair** pipes = create_pipes(X + 1);
            fclose(pipes_log_file);
            for (int i = 1; i <= X; i++) {
                if (fork() == 0) {
                    cur_pid = i;
                    child_task(pipes, cur_pid, X + 1);
                    return 0;
                }
            }
            root_task(pipes, X + 1);

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
