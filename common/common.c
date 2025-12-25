#include "common.h"
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

static const char *shared_memory_get_error = "Shared memory get error";
static const char *message_queue_get_error = "Message queue get error";
static const char *semaphore_get_error = "Semaphore get error";

SharedMemory *attach_shared_memory() {
    key_t shared_memory_key = ftok(".",SHARED_MEMORY_ID);
    if (shared_memory_key == -1) {
        perror(shared_memory_get_error);
        return NULL;
    }

    int shared_memory = shmget(shared_memory_key, sizeof(SharedMemory), IPC_CREAT | 0666);
    if (shared_memory == -1) {
        perror(shared_memory_get_error);
        return NULL;
    }

    void *shared_memory_addr = shmat(shared_memory, NULL, 0);
    if (shared_memory_addr == (void *)-1) {
        perror("Shared memory attach error");
        return NULL;
    }

    return (SharedMemory *)shared_memory_addr;
}

void detach_shared_memory(SharedMemory *shared_memory) {
    if (shmdt((void *)shared_memory) == -1)
        perror("Shared memory detach error");
}

int get_message_queue() {
    key_t message_queue_key = ftok(".",MESSAGE_QUEUE_ID);
    if (message_queue_key == -1) {
        perror(message_queue_get_error);
        return -1;
    }

    int message_queue = msgget(message_queue_key, IPC_CREAT | 0666);
    if (message_queue == -1)
        perror(message_queue_get_error);

    return message_queue;
}

int get_semaphores() {
    key_t semaphore_key = ftok(".",SEMAPHORES_ID);
    if (semaphore_key == -1) {
        perror(semaphore_get_error);
        return -1;
    }

    int semaphores = semget(semaphore_key, NSEMAPHORES, IPC_CREAT | 0666);
    if (semaphores == -1)
        perror(semaphore_get_error);

    return semaphores;
}
