#include "cave_simulation.h"

#include <linux/limits.h>
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

    if (pipe(cave_simulation->shared_memory->catwalk1_pipe) == -1
            || pipe(cave_simulation->shared_memory->catwalk2_pipe) == -1 ) {
        perror("cave_simulation_init: pipe");
        destroy_semaphores(semaphores);
        destroy_message_queue(message_queue);
        destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
        return CAVE_SIMULATION_INIT_FAIL;
    }

    return CAVE_SIMULATION_SUCCESS;
}

static void close_catwalk_pipe_input(const SharedMemory *shared_memory) {
    if (close(shared_memory->catwalk1_pipe[1]) == -1)
        perror("close_catwalk_pipe_input: close (Catwalk1)");

    if (close(shared_memory->catwalk2_pipe[1]) == -1)
        perror("close_catwalk_pipe_input: close (Catwalk2)");
}

static void close_catwalk_pipe_output(const SharedMemory *shared_memory) {
    if (close(shared_memory->catwalk1_pipe[0]) == -1)
        perror("close_catwalk_pipe_output: close (Catwalk1)");

    if (close(shared_memory->catwalk2_pipe[0]) == -1)
        perror("close_catwalk_pipe_output: close (Catwalk2)");
}

CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation) {
    bool error = false;

    // Now do not wait for the ticket clerk and guides
    for (int i = 0; i < cave_simulation->child_processes - (1 + 2); i++) {
        if (wait(NULL) == -1) {
            perror("cave_simulation_destroy: wait");
            error = true;
        }
    }

    Message message = {0};
    strcpy(message.mtext, "terminate");

    message.mtype = cave_simulation->shared_memory->ticket_clerk_pid;
    if (msgsnd(cave_simulation->message_queue, (const void *)&message, sizeof(message.mtext), 0) == -1)
        perror("cave_simulation_destroy: msgsnd (TicketClerk)");

    message.mtype = cave_simulation->shared_memory->guide1_pid;
    if (msgsnd(cave_simulation->message_queue, (const void *)&message, sizeof(message.mtext), 0) == -1)
        perror("cave_simulation_destroy: msgsnd (Guide1)");

    message.mtype = cave_simulation->shared_memory->guide2_pid;
    if (msgsnd(cave_simulation->message_queue, (const void *)&message, sizeof(message.mtext), 0) == -1)
        perror("cave_simulation_destroy: msgsnd (Guide2)");

    for (int i = 0; i < 1 + 2; i++) {
        if (wait(NULL) == -1) {
            perror("cave_simulation_destroy: wait");
            error = true;
        }
    }

    close_catwalk_pipe_input(cave_simulation->shared_memory);
    close_catwalk_pipe_output(cave_simulation->shared_memory);

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

static void init_parameters(CaveSimulation *cave_simulation) {
    cave_simulation->shared_memory->K = 5;
}

static int spawn_guide(CaveSimulation *cave_simulation) {
    int fork_res = fork();
    if (fork_res == -1) {
        perror("spawn_guide: fork");
        return -1;
    }
    if (fork_res == 0) {
        signal(SIGINT, SIG_IGN);

        close_catwalk_pipe_input(cave_simulation->shared_memory);

        if (execl("./Guide", "Guide", NULL) == -1) {
            perror("spawn_guide: execl =");
            return -1;
        }
    }
    cave_simulation->child_processes++;

    return fork_res;
}

CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation) {
    output_log(cave_simulation->semaphores, cave_simulation->shared_memory,
            "Running cave simulation (PID: %d)", getpid());
    
    init_parameters(cave_simulation);

    int fork_res = fork();
    if (fork_res == -1) {
        perror("cave_simulation_run: fork (TicketClerk)");
        return CAVE_SIMULATION_RUN_FAIL;
    }
    if (fork_res == 0) {
        signal(SIGINT, SIG_IGN);

        close_catwalk_pipe_input(cave_simulation->shared_memory);
        close_catwalk_pipe_output(cave_simulation->shared_memory);

        if (execl("./TicketClerk", "TicketClerk", NULL) == -1) {
            perror("cave_simulation_run: execl (TicketClerk)");
            return CAVE_SIMULATION_RUN_FAIL;
        }
    }
    cave_simulation->child_processes++;
    cave_simulation->shared_memory->ticket_clerk_pid = fork_res;

    int res1 = spawn_guide(cave_simulation);
    int res2 = spawn_guide(cave_simulation);
    if (res1 == -1 || res2 == -1)
        return CAVE_SIMULATION_RUN_FAIL;

    cave_simulation->shared_memory->guide1_pid = res1;
    cave_simulation->shared_memory->guide2_pid = res2;

    uint64_t time = 0;

    do {
        fork_res = fork();
        if (fork_res == -1) {
            perror("cave_simulation_run: fork (Visitor)");
            return CAVE_SIMULATION_RUN_FAIL;
        }
        if (fork_res == 0) {
            signal(SIGINT, SIG_IGN);

            close_catwalk_pipe_output(cave_simulation->shared_memory);

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
