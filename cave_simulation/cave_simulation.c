#include "cave_simulation.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/time.h>
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
    // Get time for random seed (microseconds)
    struct timeval time;
    gettimeofday(&time, NULL);
    srand(time.tv_usec);

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

    cave_simulation->shared_memory->terminating = true;

    Message message = {0};
    strcpy(message.mtext, "terminate");

    message.mtype = cave_simulation->shared_memory->ticket_clerk_pid;
    if (msgsnd(cave_simulation->message_queue, (const void *)&message, sizeof(message.mtext), 0) == -1)
        perror("cave_simulation_destroy: msgsnd (TicketClerk)");

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
        perror("cave_simulation_run: fork (TicketClerk)");
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

    uint64_t time = 0;

    do {
        take_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);
        cave_simulation->shared_memory->visitors_approaching++;
        give_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);

        fork_res = fork();
        if (fork_res == -1) {
            perror("cave_simulation_run: fork (Visitor)");
            return CAVE_SIMULATION_RUN_FAIL;
        }
        if (fork_res == 0) {
            signal(SIGINT, SIG_IGN);
            if (execl("./Visitor", "Visitor", NULL) == -1) {
                perror("cave_simulation_run: execl (Visitor)");
                return CAVE_SIMULATION_RUN_FAIL;
            }
        }
        cave_simulation->child_processes++;

        uint64_t wait_time = rand() % (CAVE_SIMULATION_MAX_VISITORS_DELAY * 1000);
        usleep(wait_time);

        time += wait_time;

        take_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);
        for (int i = 0; i < cave_simulation->shared_memory->visitors_finished; i++) {
            if (wait(NULL) == -1)
                perror("cave_simulation_run: wait");
            else
                cave_simulation->child_processes--;
        }
        cave_simulation->shared_memory->visitors_finished = 0;
        give_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);
    } while (!cave_simulation->terminating && time < 1000 * 1000 * 20);

    return CAVE_SIMULATION_SUCCESS;
}

void cave_simulation_terminate(CaveSimulation *cave_simulation) {
    cave_simulation->terminating = true;
}
