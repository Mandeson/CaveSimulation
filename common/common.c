#include "common.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>

SharedMemory *attach_shared_memory() {
    key_t shared_memory_key = ftok(".",SHARED_MEMORY_ID);
    if (shared_memory_key == -1) {
        perror("attach_shared_memory: ftok");
        return NULL;
    }

    int shared_memory = shmget(shared_memory_key, sizeof(SharedMemory), 0600);
    if (shared_memory == -1) {
        perror("attach_shared_memory: shmget");
        return NULL;
    }

    void *shared_memory_addr = shmat(shared_memory, NULL, 0);
    if (shared_memory_addr == (void *)-1) {
        perror("attach_shared_memory: shmat");
        return NULL;
    }

    return (SharedMemory *)shared_memory_addr;
}

int detach_shared_memory(SharedMemory **shared_memory) {
    SharedMemory *ptr = *shared_memory;
    *shared_memory = NULL;
    if (shmdt((void *)ptr) == -1)
        perror("detach_shared_memory: shmdt");
    return 0;
}

int get_message_queue(int id) {
    key_t message_queue_key = ftok(".", id);
    if (message_queue_key == -1) {
        perror("get_message_queue: ftok");
        return -1;
    }

    int message_queue = msgget(message_queue_key, 0600);
    if (message_queue == -1)
        perror("get_message_queue: msgget");

    return message_queue;
}

int get_semaphores() {
    key_t semaphore_key = ftok(".",SEMAPHORES_ID);
    if (semaphore_key == -1) {
        perror("get_semaphores: ftok");
        return -1;
    }

    int semaphores = semget(semaphore_key, NSEMAPHORES, 0200);
    if (semaphores == -1)
        perror("get_semaphores: semget");

    return semaphores;
}

int take_semaphore(int semaphores, int number) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = -1;
    op.sem_flg = 0;

    while (semop(semaphores, &op, 1) == -1) {
        if (errno != EINTR) {
            perror("take_semaphore: semop");
            return -1;
        }
    }

    return 0;
}

int give_semaphore(int semaphores, int number) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = 1;
    op.sem_flg = 0;

    while (semop(semaphores, &op, 1) == -1) {
        if (errno != EINTR) {
            perror("give_semaphore: semop");
            return -1;
        }
    }

    return 0;
}

int message_queue_send(int message_queue, long type, const void *data, size_t size, const char *caller) {
    Message message = {0};
    if (size > sizeof(message.mtext)) {
        char error[64];
        snprintf(error, sizeof(error) - 1, "Error: %s: message too long", caller);
        perror(error);
        return MESSAGE_QUEUE_SEND_FAIL;
    }
    message.mtype = type;
    if (data)
        memcpy(message.mtext, data, size);

    struct msqid_ds buf;

    if (msgctl(message_queue, IPC_STAT, &buf) == -1) {
        perror("msgctl");
        return MESSAGE_QUEUE_SEND_FAIL;
    }
    if (buf.msg_cbytes + sizeof(message.mtext) > buf.msg_qbytes) {
        fprintf(stderr, "Warning: %s: message queue full", caller);
    }

    while (msgsnd(message_queue, &message, sizeof(message.mtext), 0) == -1) {
        if (errno != EINTR) {
            char error[64];
            snprintf(error, sizeof(error) - 1, "Error: %s: msgsnd", caller);
            perror(error);
            return MESSAGE_QUEUE_SEND_FAIL;
        }
    }

    return MESSAGE_QUEUE_SEND_SUCCESS;
}

// Prevents message queue overflow
int message_queue_send_check_ovf(int message_queue, long type, const void *data, size_t size) {
    while (1) {
        struct msqid_ds buf;
        if (msgctl(message_queue, IPC_STAT, &buf) == -1) {
            perror("message_queue_send_margin: msgctl");
            return MESSAGE_QUEUE_SEND_FAIL;
        }

        Message message;
        if (buf.msg_cbytes + MESSAGE_QUEUE_MARGIN * sizeof(message.mtext) <= buf.msg_qbytes)
            break;

        safe_usleep(100);
    }

    return message_queue_send(message_queue, type, data, size, "message_queue_send_check_ovf");
}

int message_queue_receive(int message_queue, long type, Message *message, const char *caller, bool block) {
    int res;
    while ((res = msgrcv(message_queue, message, sizeof(message->mtext), type, block ? 0 : IPC_NOWAIT)) == -1) {
        if (errno == ENOMSG)
            return MESSAGE_QUEUE_RECEIVE_NO_MESSAGE;

        if (errno != EINTR) {
            char error[64];
            snprintf(error, sizeof(error) - 1, "Error: %s: msgrcv", caller);
            perror(error);
            return MESSAGE_QUEUE_RECEIVE_FAIL;
        }
    }
    if (res != sizeof(message->mtext)) {
        fprintf(stderr, "Error: %s: msgrcv: too few bytes received\n", caller);
        return MESSAGE_QUEUE_RECEIVE_FAIL;
    }

    return MESSAGE_QUEUE_RECEIVE_SUCCESS;
}

void close_catwalk_pipe_input(const SharedMemory *shared_memory) {
    if (close(shared_memory->catwalk_pipe[0][1]) == -1)
        perror("close_catwalk_pipe_input: close (Catwalk1)");

    if (close(shared_memory->catwalk_pipe[1][1]) == -1)
        perror("close_catwalk_pipe_input: close (Catwalk2)");
}

void close_catwalk_pipe_output(const SharedMemory *shared_memory) {
    if (close(shared_memory->catwalk_pipe[0][0]) == -1)
        perror("close_catwalk_pipe_output: close (Catwalk1)");

    if (close(shared_memory->catwalk_pipe[1][0]) == -1)
        perror("close_catwalk_pipe_output: close (Catwalk2)");
}

void safe_usleep(long microseconds) {
    while (usleep(microseconds) == -1) {
        if (errno != EINTR)
            perror("safe_usleep: usleep");
    }
}

