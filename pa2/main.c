#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common.h"
#include "ipc.h"
#include "banking.h"
#include "pipes.h"
#include "pa2345.h"

fd_pair** pipes; // pipe matrix
FILE* events_log_file;
FILE* pipes_log_file;
local_id X;
local_id cur_id = 0;
BalanceHistory balance_history = {0};

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

/** Receive messages from all processes */
int receive_all(void* self, Message* msg, MessageType type, AllHistory* history) {
    for (local_id i = 1; i <= X; i++) {
        if (i != cur_id) {
            memset(msg, 0, MAX_MESSAGE_LEN);
            if (receive(((fd_pair**) self)[i], cur_id, msg) != 0) {
                return -1;
            }
            if (history) {
                // handle
            }
        }
    }
    return 0;
}

int receive_any(void * self, Message * msg) {
    return 0;
}

/** Fill header of a message */
void init_message_header(Message* msg, MessageType type) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_payload_len = 0;
    msg->s_header.s_local_time = get_physical_time();
    msg->s_header.s_type = type;
}

BalanceState get_balance_state(void) {
    return balance_history.s_history[balance_history.s_history_len - 1];
}

void update_balance(balance_t balance) {
    timestamp_t time = get_physical_time();
    if (balance_history.s_history_len) {
        BalanceState state = get_balance_state();
        for (timestamp_t i = (timestamp_t) (state.s_time + 1); i < time; i++) {
            balance_history.s_history[balance_history.s_history_len++] = (BalanceState) {state.s_balance, i};
        }
    }
    balance_history.s_history[balance_history.s_history_len++] = (BalanceState) {balance, time};
}

void transfer(void* parent_data, local_id src, local_id dst, balance_t amount) {
    TransferOrder order = (TransferOrder) {src, dst, amount};
    Message* msg = (Message*) parent_data;

    init_message_header(msg, TRANSFER);
    memcpy(msg->s_payload, &order, sizeof(TransferOrder));
    msg->s_header.s_payload_len = sizeof(TransferOrder);

    send(&pipes[PARENT_ID], src, msg); // send TRANSFER to src
    while (receive(&pipes[dst], PARENT_ID, msg) != 0); // receive ACK from dst

    if (msg->s_header.s_type != ACK) {
        printf(received_wrong_type_fmt, msg->s_header.s_type);
        exit(-1);
    }
}

/** Execute parent or child task */
void task(bool isChild) {
    close_unused_pipes();
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);

    if (isChild) {
        uint8_t running = X;
        while (running) {
            receive_any(pipes, msg);
            if (msg->s_header.s_type == TRANSFER) {
                TransferOrder* order = (TransferOrder*) msg->s_payload;
                if (last_sender_id == PARENT_ID) {
                    update_balance((balance_t) (get_balance_state().s_balance - order->s_amount));
                    send(&pipes[cur_id], order->s_dst, msg);
                    printf(log_transfer_out_fmt, get_physical_time(), cur_id, order->s_amount, order->s_dst);
                    fprintf(events_log_file, log_transfer_out_fmt, get_physical_time(), cur_id, order->s_amount,
                            order->s_dst);
                } else {
                    update_balance((balance_t) (get_balance_state().s_balance + order->s_amount));
                    init_message_header(msg, ACK); // payload isn't touched, so pointer to order is valid
                    send(&pipes[cur_id], PARENT_ID, msg);
                    printf(log_transfer_in_fmt, get_physical_time(), cur_id, order->s_amount, order->s_src);
                    fprintf(events_log_file, log_transfer_in_fmt, get_physical_time(), cur_id, order->s_amount,
                            order->s_src);
                }
            } else if (msg->s_header.s_type == STOP || msg->s_header.s_type == DONE) {
                running--;
            } else {
                printf(received_wrong_type_fmt, msg->s_header.s_type);
                exit(-1);
            }
        }

        init_message_header(msg, BALANCE_HISTORY);
        memcpy(msg->s_payload, &balance_history, sizeof(BalanceHistory));
        msg->s_header.s_payload_len = sizeof(BalanceHistory);
        send(&pipes[cur_id], PARENT_ID, msg);
    } else {
        receive_all(pipes, msg, STARTED, NULL); // receiving STARTED messages
        bank_robbery(msg, X);
        init_message_header(msg, STOP);
        send_multicast(pipes, msg); // sending STOP messages
        receive_all(pipes, msg, DONE, NULL); // receiving DONE messages

        AllHistory* history = malloc(sizeof(AllHistory));
        receive_all(pipes, msg, BALANCE_HISTORY, history); // receiving BALANCE_HISTORY messages
        print_history(history);
        free(history);
        while (wait(NULL) > 0);
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
                    update_balance((balance_t) atoi(argv[i + 2]));
                    task(true);
                    return 0;
                }
            }
            task(false);
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
