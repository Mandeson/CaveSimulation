#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include "cave_simulation.h"
#include "common.h"

static const char *shared_memory_creation_error = "Shared memory creation error";
static const char *message_queue_creation_error = "Message queue creation error";
static const char *semaphore_creation_error = "Semaphore creation error";

static int create_shared_memory() {
    key_t shared_memory_key = ftok(".",SHARED_MEMORY_ID);
    if (shared_memory_key == -1) {
        perror(shared_memory_creation_error);
        return -1;
    }

    int shared_memory = shmget(shared_memory_key, sizeof(SharedMemory), IPC_CREAT | IPC_EXCL | 0666);
    if (shared_memory == -1) {
        perror(shared_memory_creation_error);
        return -1;
    }

    void *shared_memory_addr = shmat(shared_memory, NULL, 0);
    if (shared_memory_addr == (void *)-1) {
        shmctl(shared_memory, IPC_RMID, NULL);
        perror("Shared memory attach error");
        return -1;
    }

    memset(shared_memory_addr, 0, sizeof(SharedMemory));

    shmdt(shared_memory_addr);

    return shared_memory;
}

static int create_message_queue() {
    key_t message_queue_key = ftok(".",MESSAGE_QUEUE_ID);
    if (message_queue_key == -1) {
        perror(message_queue_creation_error);
        return -1;
    }

    int message_queue = msgget(message_queue_key, IPC_CREAT | IPC_EXCL | 0666);
    if (message_queue == -1)
        perror(message_queue_creation_error);

    return message_queue;
}

static int create_semaphores() {
    key_t semaphore_key = ftok(".",SEMAPHORES_ID);
    if (semaphore_key == -1) {
        perror(semaphore_creation_error);
        return -1;
    }

    int semaphores = semget(semaphore_key, NSEMAPHORES, IPC_CREAT | IPC_EXCL | 0666);
    if (semaphores == -1)
        perror(semaphore_creation_error);

    return semaphores;
}

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation) {
    int shared_memory = create_shared_memory();
    if (shared_memory == -1)
        return CAVE_SIMULATION_INIT_FAIL;
    cave_simulation->shared_memory = shared_memory;

    int message_queue = create_message_queue();
    if (message_queue == -1) {
        shmctl(cave_simulation->shared_memory, IPC_RMID, NULL);
        return CAVE_SIMULATION_INIT_FAIL;
    }
    cave_simulation->message_queue = message_queue;

    int semaphores = create_semaphores();
    if (semaphores == -1) {
        shmctl(cave_simulation->shared_memory, IPC_RMID, NULL);
        msgctl(cave_simulation->message_queue, IPC_RMID, NULL);
        return CAVE_SIMULATION_INIT_FAIL;
    }
    cave_simulation->semaphores = semaphores;

    return CAVE_SIMULATION_SUCCESS;
}

CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation) {
    bool error = false;

    if (semctl(cave_simulation->semaphores, NSEMAPHORES, IPC_RMID, NULL) == -1) {
        perror("Semaphore remove error");
        error = true;
    }

    if (msgctl(cave_simulation->message_queue, IPC_RMID, NULL) == -1) {
        perror("Message queue remove error");
        error = true;
    }

    if (shmctl(cave_simulation->shared_memory, IPC_RMID, NULL) == -1) {
        perror("Shared memory remove error");
        error = true;
    }

    return error ? CAVE_SIMULATION_DESTROY_FAIL : CAVE_SIMULATION_SUCCESS;
}

CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation) {
    return CAVE_SIMULATION_SUCCESS;
}
