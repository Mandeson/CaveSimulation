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

    int shared_memory = shmget(shared_memory_key, sizeof(SharedMemory), IPC_CREAT | 0600);
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

int detach_shared_memory(SharedMemory *shared_memory) {
    int res = shmdt((void *)shared_memory);
    if (res == -1)
        perror("detach_shared_memory: shmdt");
    return res;
}

int get_message_queue(int id) {
    key_t message_queue_key = ftok(".", id);
    if (message_queue_key == -1) {
        perror("get_message_queue: ftok");
        return -1;
    }

    int message_queue = msgget(message_queue_key, IPC_CREAT | 0600);
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

    int semaphores = semget(semaphore_key, NSEMAPHORES, IPC_CREAT | 0600);
    if (semaphores == -1)
        perror("get_semaphores: semget");

    return semaphores;
}

int take_semaphore(int semaphores, int number) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = -1;
    op.sem_flg = 0;//SEM_UNDO;

    while (semop(semaphores, &op, 1) == -1) {
        if (errno != EINTR) {
            perror("take_semaphore: semop");
            return -1;
        }
    }

    // if (number == SHARED_MEMORY_SEMAPHORE) {
    //     take_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    //     printf("PID: %d takes shm semaphore\n", getpid());
    //     fflush(stdout);
    //     give_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    // }

    return 0;
}

int take_semaphore_n(int semaphores, int number, int n) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = -n;
    op.sem_flg = 0;

    if (semop(semaphores, &op, 1) == -1) {
        perror("take_semaphore_n: semop");
        return -1;
    }

    return 0;
}

int give_semaphore(int semaphores, int number) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = 1;
    op.sem_flg = 0;//SEM_UNDO;

    // if (number == SHARED_MEMORY_SEMAPHORE) {
    //     take_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    //     printf("PID: %d gives shm semaphore\n", getpid());
    //     fflush(stdout);
    //     give_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    // }

    while (semop(semaphores, &op, 1) == -1) {
        if (errno != EINTR) {
            perror("give_semaphore: semop");
            return -1;
        }
    }

    return 0;
}

int give_semaphore_n(int semaphores, int number, int n) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = n;
    op.sem_flg = 0;

    if (semop(semaphores, &op, 1) == -1) {
        perror("give_semaphore_n: semop");
        return -1;
    }

    return 0;
}

int set_semaphore(int semaphores, int number, int n) {
    while (semctl(semaphores, number, SETVAL, n) == -1) {
        if (errno != EINTR) {
            char error[64];
            snprintf(error, sizeof(error) - 1, "set_semaphore: semctl");
            perror(error);
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
