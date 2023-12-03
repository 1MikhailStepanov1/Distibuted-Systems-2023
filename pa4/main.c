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

/** Fill header of a message */
void init_message_header(Message* msg, MessageType type) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_payload_len = 0;
    msg->s_header.s_local_time = inc_and_get_lamport_time();
    msg->s_header.s_type = type;
}

BalanceState get_balance_state(void) {
    return balance_history.s_history[balance_history.s_history_len - 1];
}

void update_balance(balance_t balance) {
    timestamp_t time = get_lamport_time();
    if (balance_history.s_history_len) {
        BalanceState state = get_balance_state();
        for (timestamp_t i = (timestamp_t) (state.s_time + 1); i < time; i++) {
            balance_history.s_history[balance_history.s_history_len++] = (BalanceState) {state.s_balance, i};
        }
    }
    balance_history.s_history[balance_history.s_history_len++] = (BalanceState) {balance, time};
}

void transfer(__attribute__((unused)) void* parent_data, local_id src, local_id dst, balance_t amount) {
    TransferOrder order = (TransferOrder) {src, dst, amount};
    Message* msg = malloc(MAX_MESSAGE_LEN);
    init_message_header(msg, TRANSFER);
    memcpy(msg->s_payload, &order, sizeof(TransferOrder));
    msg->s_header.s_payload_len = sizeof(TransferOrder);

    send(pipes[PARENT_ID], src, msg); // send TRANSFER to src
    while (1) {
        int result = receive(pipes[dst], PARENT_ID, msg); // receive ACK from dst
        if (result == -1) {
            printf("Transfer failed!");
            exit(-1);
        } else if (result == 0) {
            break;
        }
    }

    if (msg->s_header.s_type != ACK) {
        printf(received_wrong_type_fmt, msg->s_header.s_type);
        exit(-1);
    }
    free(msg);
}

/** Receive messages from all processes */
int receive_all(void* self, MessageType type, AllHistory* history) {
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
            if (history) {
                memcpy(&history->s_history[i - 1], msg->s_payload, sizeof(BalanceHistory));
            }
        }
    }
    free(msg);
    return 0;
}

/** Task of client (parent) */
void client_task(void) {
    close_unused_pipes();

    receive_all(pipes, STARTED, NULL); // receiving STARTED messages
    bank_robbery(NULL, X);

    Message* msg = calloc(MAX_MESSAGE_LEN, 1);
    init_message_header(msg, STOP);
    send_multicast(pipes, msg); // sending STOP messages
    free(msg);

    receive_all(pipes, DONE, NULL); // receiving DONE messages

    AllHistory* history = calloc(1, sizeof(AllHistory));
    history->s_history_len = X;
    receive_all(pipes, BALANCE_HISTORY, history); // receiving BALANCE_HISTORY messages
    print_history(history);
    free(history);

    while (wait(NULL) > 0);

    fclose(events_log_file);
    close_pipes();
}

/** Task of account (child) */
void account_task(void) {
    close_unused_pipes();
    Message* msg = calloc(MAX_MESSAGE_LEN, 1);

    init_message_header(msg, STARTED);
    sprintf(msg->s_payload, log_started_fmt, get_lamport_time(), cur_id, getpid(), getppid(),
            get_balance_state().s_balance);
    msg->s_header.s_payload_len = strlen(msg->s_payload);
    send_multicast(pipes, msg); // send STARTED to everyone

    receive_all(pipes, STARTED, NULL); // receive STARTED from everyone
    printf(log_received_all_started_fmt, get_lamport_time(), cur_id);
    fprintf(events_log_file, log_received_all_started_fmt, get_lamport_time(), cur_id);

    uint8_t running = X;
    while (running) {
        receive_any(pipes, msg);
        sync_lamport_time(msg->s_header.s_local_time);
        inc_and_get_lamport_time();
        if (msg->s_header.s_type == TRANSFER) {
            TransferOrder* order = (TransferOrder*) msg->s_payload;
            if (last_sender_id == PARENT_ID) {
                msg->s_header.s_local_time = inc_and_get_lamport_time();
                update_balance((balance_t)(get_balance_state().s_balance - order->s_amount));
                balance_history.s_history[balance_history.s_history_len - 1].s_balance_pending_in = order->s_amount;
                send(pipes[cur_id], order->s_dst, msg);
                printf(log_transfer_out_fmt, get_lamport_time(), cur_id, order->s_amount, order->s_dst);
                fprintf(events_log_file, log_transfer_out_fmt, get_lamport_time(), cur_id, order->s_amount,
                        order->s_dst);
            } else {
                update_balance((balance_t)(get_balance_state().s_balance + order->s_amount));
                init_message_header(msg, ACK); // payload isn't touched, so pointer to order is valid
                send(pipes[cur_id], PARENT_ID, msg);
                printf(log_transfer_in_fmt, get_lamport_time(), cur_id, order->s_amount, order->s_src);
                fprintf(events_log_file, log_transfer_in_fmt, get_lamport_time(), cur_id, order->s_amount,
                        order->s_src);
            }
        } else if (msg->s_header.s_type == STOP) {
            init_message_header(msg, DONE);
            sprintf(msg->s_payload, log_done_fmt, get_lamport_time(), cur_id, get_balance_state().s_balance);
            msg->s_header.s_payload_len = strlen(msg->s_payload);
            send_multicast(pipes, msg); // send DONE to everyone
            running--;
        } else if (msg->s_header.s_type == DONE) {
            running--;
        } else {
            printf(received_wrong_type_fmt, msg->s_header.s_type);
            exit(-1);
        }
    }

    update_balance(get_balance_state().s_balance); // fill gaps in history if present
    init_message_header(msg, BALANCE_HISTORY);
    memcpy(msg->s_payload, &balance_history, sizeof(BalanceHistory));
    msg->s_header.s_payload_len = sizeof(BalanceHistory);
    send(pipes[cur_id], PARENT_ID, msg); // send BALANCE_HISTORY to client

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
                    balance_history.s_id = cur_id;
                    update_balance((balance_t) atoi(argv[i + 2]));
                    account_task();
                    return 0;
                }
            }
            client_task();
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
