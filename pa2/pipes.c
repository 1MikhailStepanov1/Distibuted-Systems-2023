#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include "ipc.h"
#include "pipes.h"

const char *log_pipe_open_fmt = "Pipe (%d; %d) created. read_fd=%d, write_fd=%d\n";

extern fd_pair **pipes;
extern FILE *events_log_file;
extern FILE *pipes_log_file;
extern local_id X;
extern local_id cur_id;

void create_pipes(void) {
    pipes = malloc(sizeof(fd_pair *) * (X + 1));
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
