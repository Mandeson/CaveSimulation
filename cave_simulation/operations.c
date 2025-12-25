#include "operations.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

int create_shared_memory(SharedMemory **shared_memory_ptr) {
    key_t shared_memory_key = ftok(".",SHARED_MEMORY_ID);
    if (shared_memory_key == -1) {
        perror("create_shared_memory: ftok");
        return -1;
    }

    int shared_memory_id = shmget(shared_memory_key, sizeof(SharedMemory), IPC_CREAT | IPC_EXCL | 0666);
    if (shared_memory_id == -1) {
        perror("create_shared_memory: shmget");
        return -1;
    }

    void *shared_memory_addr = shmat(shared_memory_id, NULL, 0);
    if (shared_memory_addr == (void *)-1) {
        perror("create_shared_memory: shmat");
        shmctl(shared_memory_id, IPC_RMID, NULL);
        return -1;
    }

    SharedMemory *shared_memory = (SharedMemory *)shared_memory_addr;
    *shared_memory_ptr = shared_memory;

    return shared_memory_id;
}

int destroy_shared_memory(SharedMemory **shared_memory_ptr, int shared_memory_id) {
    if (shmdt(*shared_memory_ptr) == -1) {
        perror("destroy_shared_memory: shmdt");
        return -1;
    }
    *shared_memory_ptr = NULL;

    if (shmctl(shared_memory_id, IPC_RMID, NULL) == -1) {
        perror("destroy_shared_memory: shmctl");
        return -1;
    }

    return 0;
}

int create_message_queue() {
    key_t message_queue_key = ftok(".",MESSAGE_QUEUE_ID);
    if (message_queue_key == -1) {
        perror("create_message_queue: ftok");
        return -1;
    }

    int message_queue = msgget(message_queue_key, IPC_CREAT | IPC_EXCL | 0666);
    if (message_queue == -1)
        perror("create_message_queue: msgget");

    return message_queue;
}

int destroy_message_queue(int message_queue) {
    int res = msgctl(message_queue, IPC_RMID, NULL);
    if (res == -1)
        perror("destroy_message_queue: msgctl");

    return res;
}

int create_semaphores() {
    key_t semaphore_key = ftok(".",SEMAPHORES_ID);
    if (semaphore_key == -1) {
        perror("create_semaphores: ftok");
        return -1;
    }

    int semaphores = semget(semaphore_key, NSEMAPHORES, IPC_CREAT | IPC_EXCL | 0666);
    if (semaphores == -1)
        perror("create_semaphores: semget");

    return semaphores;
}

int destroy_semaphores(int semaphores) {
    int res = semctl(semaphores, NSEMAPHORES, IPC_RMID, NULL);
    if (res == -1)
        perror("destroy_message_queue: semctl");

    return res;
}

int create_output_file(SharedMemory *shared_memory) {
    int fd = open(shared_memory->output_file_name, O_CREAT, 0600);
    if (fd == -1) {
        perror("create_output_file: open");
        return -1;
    }
    if (close(fd) == -1) {
        perror("create_output_file: close");
        return -1;
    }

    return 0;
}
