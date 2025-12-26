#include "cave_simulation.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "operations.h"

static void init_shared_memory(SharedMemory *shared_memory) {
    memset(shared_memory, 0, sizeof(SharedMemory));

    // Get time to set the output file name
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(shared_memory->output_file_name, sizeof(shared_memory->output_file_name),
            "CaveSimulation_log_%d-%02d-%02d_%02d:%02d:%02d.txt", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static int init_semaphores(int semaphores) {
    if (semctl(semaphores, OUTPUT_LOG_SEMAPHORE, SETVAL, 1) == -1) {
        perror("init_semaphores: semctl (OUTPUT_LOG_SEMAPHORE)");
        return -1;
    }

    if (semctl(semaphores, SHARED_MEMORY_SEMAPHORE, SETVAL, 1) == -1) {
        perror("init_semaphores: semctl (SHARED_MEMORY_SEMAPHORE)");
        return -1;
    }

    return 0;
}

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation) {
    int shared_memory = create_shared_memory(&cave_simulation->shared_memory);
    if (shared_memory == -1)
        return CAVE_SIMULATION_INIT_FAIL;
    cave_simulation->shared_memory_id = shared_memory;
    init_shared_memory(cave_simulation->shared_memory);

    int message_queue = create_message_queue();
    if (message_queue == -1) {
        destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
        return CAVE_SIMULATION_INIT_FAIL;
    }
    cave_simulation->message_queue = message_queue;

    int semaphores = create_semaphores();
    if (semaphores == -1) {
        destroy_message_queue(message_queue);
        destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
        return CAVE_SIMULATION_INIT_FAIL;
    }
    cave_simulation->semaphores = semaphores;
    if (init_semaphores(semaphores) == -1) {
        destroy_semaphores(semaphores);
        destroy_message_queue(message_queue);
        destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
        return CAVE_SIMULATION_INIT_FAIL;
    }

    if (create_output_file(cave_simulation->shared_memory) == -1) {
        destroy_semaphores(semaphores);
        destroy_message_queue(message_queue);
        destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
        return CAVE_SIMULATION_INIT_FAIL;
    }

    return CAVE_SIMULATION_SUCCESS;
}

CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation) {
    bool error = false;

    for (int i = 0; i < cave_simulation->child_processes; i++) {
        if (wait(NULL) == -1) {
            perror("cave_simulation_destroy: wait");
            error = true;
        }
    }

    output_log(cave_simulation->semaphores, cave_simulation->shared_memory,
            "Destroying cave simulation (PID: %d)", getpid());
    
    if (destroy_semaphores(cave_simulation->semaphores) == -1)
        error = true;

    if (destroy_message_queue(cave_simulation->message_queue) == -1)
        error = true;

    if (destroy_shared_memory(&cave_simulation->shared_memory,
            cave_simulation->shared_memory_id) == -1) {
        error = true;
    }

    return error ? CAVE_SIMULATION_DESTROY_FAIL : CAVE_SIMULATION_SUCCESS;
}

CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation) {
    output_log(cave_simulation->semaphores, cave_simulation->shared_memory,
            "Running cave simulation (PID: %d)", getpid());

    int fork_res = fork();
    if (fork_res == -1) {
        perror("cave_simulation_run: fork");
        return CAVE_SIMULATION_RUN_FAIL;
    }
    if (fork_res == 0) {
        if (execl("./TicketClerk", "TicketClerk", NULL) == -1) {
            perror("cave_simulation_run: execl (TicketClerk)");
            return CAVE_SIMULATION_RUN_FAIL;
        }
    }
    cave_simulation->child_processes++;
    cave_simulation->shared_memory->ticket_clerk_pid = fork_res;

    return CAVE_SIMULATION_SUCCESS;
}
