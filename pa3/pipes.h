#ifndef PA2_PIPES_H
#define PA2_PIPES_H

/** Struct which represents descriptors of pipe */
typedef struct fd_pair {
    int fd[2];
} fd_pair;

/** Allocates space for pipe-matrix */
void create_pipes(void);

/** Close descriptors of pipes that aren't used by the process */
void close_unused_pipes(void);

/** Close all pipes after task is done and free space */
void close_pipes(void);

#endif
