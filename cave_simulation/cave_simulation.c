#include "cave_simulation.h"

#include <linux/limits.h>
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
#include <unistd.h>

#include "common.h"
#include "logger.h"
#include "operations.h"

static void init_shared_memory(SharedMemory *shared_memory) {
    memset(shared_memory, 0, sizeof(SharedMemory));
}

static int init_semaphores(int semaphores) {
    if (semctl(semaphores, SHARED_MEMORY_SEMAPHORE, SETVAL, 1) == -1) {
        perror("init_semaphores: semctl (SHARED_MEMORY_SEMAPHORE)");
        return -1;
    }

    if (semctl(semaphores, TICKET_REGULAR_SEMAPHORE, SETVAL, 0) == -1) {
        perror("init_semaphores: semctl (TICKET_REGULAR_SEMAPHORE)");
        return -1;
    }

    if (semctl(semaphores, TICKET_PRIORITY_SEMAPHORE, SETVAL, 0) == -1) {
        perror("init_semaphores: semctl (TICKET_PRIORITY_SEMAPHORE)");
        return -1;
    }

    if (semctl(semaphores, WAITING_BY_GUIDE1_SEMAPHORE, SETVAL, 1) == -1) {
        perror("init_semaphores: semctl (WAITING_BY_GUIDE1_SEMAPHORE)");
        return -1;
    }

    if (semctl(semaphores, WAITING_BY_GUIDE2_SEMAPHORE, SETVAL, 1) == -1) {
        perror("init_semaphores: semctl (WAITING_BY_GUIDE2_SEMAPHORE)");
        return -1;
    }

    return 0;
}

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation, bool log_to_stdout,
        bool skip_start_confirmation, bool disable_guard) {
    signal(SIGUSR1, SIG_IGN);

    cave_simulation->disable_guard = disable_guard;

    // Get time for random seed (microseconds)
    struct timeval time;
    gettimeofday(&time, NULL);
    srand(time.tv_usec);

    if (logger_init(&cave_simulation->logger, log_to_stdout) == -1)
        return CAVE_SIMULATION_INIT_FAIL;

    logger_interface_new(&cave_simulation->logger_interface, "CaveSimulation");

    if (!skip_start_confirmation) {
        printf("Press enter to start simulation ");
        fflush(stdout);

        char c;
        read(0, &c, 1);
    }

    logger_log(&cave_simulation->logger_interface, "Initializing cave simulation");

    int shared_memory = create_shared_memory(&cave_simulation->shared_memory);
    if (shared_memory == -1)
        return CAVE_SIMULATION_INIT_FAIL;
    cave_simulation->shared_memory_id = shared_memory;
    init_shared_memory(cave_simulation->shared_memory);

    int message_queue = create_message_queue(MESSAGE_QUEUE_ID);
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

    if (pipe(cave_simulation->shared_memory->catwalk_pipe[0]) == -1
            || pipe(cave_simulation->shared_memory->catwalk_pipe[1]) == -1 ) {
        perror("cave_simulation_init: pipe");
        destroy_semaphores(semaphores);
        destroy_message_queue(message_queue);
        destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
        return CAVE_SIMULATION_INIT_FAIL;
    }

    return CAVE_SIMULATION_SUCCESS;
}

static void close_catwalk_pipe_input(const SharedMemory *shared_memory) {
    if (close(shared_memory->catwalk_pipe[0][1]) == -1)
        perror("close_catwalk_pipe_input: close (Catwalk1)");

    if (close(shared_memory->catwalk_pipe[1][1]) == -1)
        perror("close_catwalk_pipe_input: close (Catwalk2)");
}

static void close_catwalk_pipe_output(const SharedMemory *shared_memory) {
    if (close(shared_memory->catwalk_pipe[0][0]) == -1)
        perror("close_catwalk_pipe_output: close (Catwalk1)");

    if (close(shared_memory->catwalk_pipe[1][0]) == -1)
        perror("close_catwalk_pipe_output: close (Catwalk2)");
}

CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation) {
    take_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);
    cave_simulation->shared_memory->terminating = true;
    give_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);

    logger_log(&cave_simulation->logger_interface, "Destroying cave simulation");

    if (!cave_simulation->disable_guard)
        kill(cave_simulation->guard_pid, SIGUSR1);

    bool error = false;

    // Wait only for visitors (not ticket clerk, guides only if interrupted)
    int leave_processes = 1; // Ticket clerk
    if (!cave_simulation->shared_memory->interrupted)
        leave_processes += GUIDE_COUNT;
    logger_log(&cave_simulation->logger_interface, "Waiting for visitors to finish");
    while (cave_simulation->child_processes - cave_simulation->child_processes_finished
            > leave_processes) {
        usleep(1000);
    }

    logger_log(&cave_simulation->logger_interface, "Finished destroying visitors");

    const char *message = "terminate";
    size_t len = strlen(message);

    if (message_queue_send(cave_simulation->message_queue,
            cave_simulation->shared_memory->ticket_clerk_pid, message, len,
            "cave_simulation_destroy terminate (TicketClerk)") == MESSAGE_QUEUE_SEND_FAIL)
        error = true;

    if (message_queue_send(cave_simulation->message_queue,
            cave_simulation->shared_memory->guide1_pid, message, len,
            "cave_simulation_destroy terminate (Guide1)") == MESSAGE_QUEUE_SEND_FAIL)
        error = true;

    if (message_queue_send(cave_simulation->message_queue,
            cave_simulation->shared_memory->guide2_pid, message, len,
            "cave_simulation_destroy terminate (Guide2)") == MESSAGE_QUEUE_SEND_FAIL)
        error = true;

    // Wait for ticket clerk and guides (if not interrupted)
    while (cave_simulation->child_processes > cave_simulation->child_processes_finished) {
        usleep(1000);
    }
    logger_log(&cave_simulation->logger_interface, "Total number of processes run: %d",
            cave_simulation->child_processes);
    pthread_cancel(cave_simulation->child_wait_thread);

    close_catwalk_pipe_input(cave_simulation->shared_memory);
    close_catwalk_pipe_output(cave_simulation->shared_memory);

    logger_log(&cave_simulation->logger_interface,
            "Destroying cave simulation (PID: %d)", getpid());
    
    if (destroy_semaphores(cave_simulation->semaphores) == -1)
        error = true;

    if (destroy_message_queue(cave_simulation->message_queue) == -1)
        error = true;

    if (destroy_shared_memory(&cave_simulation->shared_memory,
            cave_simulation->shared_memory_id) == -1) {
        error = true;
    }

    logger_destroy(&cave_simulation->logger);

    return error ? CAVE_SIMULATION_DESTROY_FAIL : CAVE_SIMULATION_SUCCESS;
}

static void init_parameters(CaveSimulation *cave_simulation) {
    SharedMemory *shared_memory = cave_simulation->shared_memory;
    shared_memory->N[0] = 5;
    shared_memory->N[1] = 5;
    shared_memory->T[0] = 1;
    shared_memory->T[1] = 1;
    shared_memory->K = 3;
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

static void *child_wait_thread_function(void *arg) {
    CaveSimulation *cave_simulation = (CaveSimulation *)arg;
    while (true) {
        if (wait(NULL) != -1)
            cave_simulation->child_processes_finished++;
    }
    return NULL;
}

CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation) {
    logger_log(&cave_simulation->logger_interface,
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

    setpgid(0, 0);
    signal(SIGUSR1, SIG_IGN);

    pthread_create(&cave_simulation->child_wait_thread, NULL, child_wait_thread_function, cave_simulation);

    take_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);

    int res1 = spawn_guide(cave_simulation);
    int res2 = spawn_guide(cave_simulation);
    if (res1 == -1 || res2 == -1)
        return CAVE_SIMULATION_RUN_FAIL;

    cave_simulation->shared_memory->guide1_pid = res1;
    cave_simulation->shared_memory->guide2_pid = res2;

    give_semaphore(cave_simulation->semaphores, SHARED_MEMORY_SEMAPHORE);

    if (!cave_simulation->disable_guard) {
        fork_res = fork();
        if (fork_res == -1) {
            perror("spawn_guide: fork");
            return -1;
        }
        if (fork_res == 0) {
            signal(SIGINT, SIG_IGN);

            close_catwalk_pipe_input(cave_simulation->shared_memory);
            close_catwalk_pipe_output(cave_simulation->shared_memory);

            if (execl("./Guard", "Guard", NULL) == -1) {
                perror("cave_simulation_run: execl (Guard)");
                return -1;
            }
        }
        cave_simulation->guard_pid = fork_res;
        cave_simulation->child_processes++;
    }

    uint64_t time = 0;

    do {
        if (cave_simulation->child_processes - cave_simulation->child_processes_finished + 1
                < MAX_PROCESSES) {
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
        }

        uint64_t wait_time = rand() % (CAVE_SIMULATION_MAX_VISITORS_DELAY * 1000);
        //usleep(wait_time);

        time += wait_time;
    } while (!cave_simulation->shared_memory->interrupted && time < 1000 * 1000 * 20);

    return CAVE_SIMULATION_SUCCESS;
}

void cave_simulation_terminate(CaveSimulation *cave_simulation) {
    cave_simulation->shared_memory->interrupted = true;
    signal(SIGUSR1, SIG_IGN);
    kill(0, SIGUSR1);
}
