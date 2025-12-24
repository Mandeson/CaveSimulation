#include "common.h"
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>

static const char *message_queue_get_error = "Message queue get error";
static const char *semaphore_get_error = "Semaphore get error";

int get_message_queue() {
    key_t message_queue_key = ftok(".",MESSAGE_QUEUE_ID);
    if (message_queue_key == -1) {
        perror(message_queue_get_error);
        return -1;
    }

    int message_queue = msgget(message_queue_key, IPC_CREAT | IPC_EXCL | 666);
    if (message_queue == -1)
        perror(message_queue_get_error);

    return message_queue;
}

int get_semaphores() {
    key_t semaphore_key = ftok(".",SEMAPHORE_ID);
    if (semaphore_key == -1) {
        perror(semaphore_get_error);
        return -1;
    }

    int semaphores = semget(semaphore_key, NSEMAPHORES, IPC_CREAT | IPC_EXCL | 666);
    if (semaphores == -1)
        perror(semaphore_get_error);

    return semaphores;
}
