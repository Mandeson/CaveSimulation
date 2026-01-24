#include "cave_simulation.h"

#include <errno.h>
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
#include "util/time.h"

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

    if (semctl(semaphores, CATWALK_SEMAPHORE, SETVAL, 1) == -1) {
        perror("init_semaphores: semctl (CATWALK_SEMAPHORE)");
        return -1;
    }

    return 0;
}

static void init_parameters(CaveSimulation *cave_simulation,
        const SimulationParameters *parameters) {
    // TODO: Check if parameters are valid
    cave_simulation->shared_memory->parameters = *parameters;
}

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation,
        const SimulationParameters *parameters, bool log_to_stdout,
        bool skip_start_confirmation, bool disable_guard) {
    signal(SIGUSR1, SIG_IGN);

    cave_simulation->disable_guard = disable_guard;

    // Get time for random seed (microseconds)
    struct timeval time;
    gettimeofday(&time, NULL);
    srand(time.tv_usec);

    int shared_memory = create_shared_memory(&cave_simulation->shared_memory);
    if (shared_memory == -1)
        return CAVE_SIMULATION_INIT_FAIL;
    cave_simulation->shared_memory_id = shared_memory;
    init_shared_memory(cave_simulation->shared_memory);

    if (logger_init(&cave_simulation->logger, log_to_stdout) == -1) {
        destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
        return CAVE_SIMULATION_INIT_FAIL;
    }

    logger_interface_new(&cave_simulation->logger_interface, "CaveSimulation",
            cave_simulation->shared_memory);

    init_parameters(cave_simulation, parameters);

    if (!skip_start_confirmation) {
        printf("Press enter to start the simulation ");
        fflush(stdout);

        char c;
        if (read(0, &c, 1) == -1 && errno != EINTR) {
            perror("cave_simulation_init: read (enter)");
            logger_destroy(&cave_simulation->logger);
            destroy_shared_memory(&cave_simulation->shared_memory, shared_memory);
            return CAVE_SIMULATION_INIT_FAIL;
        }
    }

    clock_init(&cave_simulation->clock, cave_simulation->shared_memory);

    logger_log(&cave_simulation->logger_interface, "Initializing cave simulation "
            "(Parameters: Tp: %d, Tk: %d, N1: %d, N2: %d, T1: %d, T2: %d, K: %d)",
            parameters->Tp, parameters->Tk, parameters->N[0], parameters->N[1],
            parameters->T[0], parameters->T[1], parameters->K);

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
    cave_simulation->shared_memory->terminating = true;

    bool interrupted = cave_simulation->shared_memory->interrupted;

    logger_log(&cave_simulation->logger_interface, "Destroying cave simulation");

    bool error = false;

    if (cave_simulation->simulation_running) {
        // Wait for all processes to attach their signal handlers
        while (cave_simulation->shared_memory->processes_starting > 0) {
            usleep(MILLISECONDS_IN_SECOND);
        }

        if (interrupted) {
            // Send SIGUSR1 to all child processes
            kill(0, SIGUSR1);
        }

        if (!interrupted && !cave_simulation->disable_guard && cave_simulation->guard_pid != 0)
            kill(cave_simulation->guard_pid, SIGUSR1);

        // If the simulation wasn't interrupted, wait only for visitors
        int leave_processes = 0;
        if (!interrupted)
            leave_processes += GUIDE_COUNT + 1; // guides + ticket clerk
        logger_log(&cave_simulation->logger_interface, "Waiting for visitors to finish");
        while (cave_simulation->child_processes - cave_simulation->child_processes_finished
                > leave_processes) {
            usleep(10000);
            logger_log(&cave_simulation->logger_interface, "Processes: %d Finished: %d", cave_simulation->child_processes,
                    cave_simulation->child_processes_finished);
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
            usleep(10000);
            logger_log(&cave_simulation->logger_interface, "Processes: %d Finished: %d", cave_simulation->child_processes,
                    cave_simulation->child_processes_finished);
        }
        logger_log(&cave_simulation->logger_interface, "Total number of child processes run: %d. Finished: %d",
                cave_simulation->child_processes, cave_simulation->child_processes_finished);
        pthread_cancel(cave_simulation->child_wait_thread);
    }

    close_catwalk_pipe_input(cave_simulation->shared_memory);
    close_catwalk_pipe_output(cave_simulation->shared_memory);

    clock_destroy(&cave_simulation->clock);

    logger_log(&cave_simulation->logger_interface,
            "Destroying cave simulation (PID: %d)", getpid());
    
    if (destroy_semaphores(cave_simulation->semaphores) == -1)
        error = true;

    if (destroy_message_queue(cave_simulation->message_queue) == -1)
        error = true;

    logger_destroy(&cave_simulation->logger);

    if (destroy_shared_memory(&cave_simulation->shared_memory,
            cave_simulation->shared_memory_id) == -1) {
        error = true;
    }

    CaveSimulationRes res = error ? CAVE_SIMULATION_DESTROY_FAIL : CAVE_SIMULATION_SUCCESS;

    if (interrupted)
        exit(res);

    return res;
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
    cave_simulation->shared_memory->processes_starting++;

    return fork_res;
}

static void *child_wait_thread_function(void *arg) {
    CaveSimulation *cave_simulation = (CaveSimulation *)arg;
    while (true) {
        int res = wait(NULL);
        if (res != -1) {
            cave_simulation->child_processes_finished++;
            logger_log(&cave_simulation->logger_interface, "Child process %d finished", res);
        }
    }
    return NULL;
}

static int random_time_between_visitors(const SimulationParameters *parameters) {
    int time = MAX((parameters->T[0] * SECONDS_IN_MINUTE + parameters->T[1] * SECONDS_IN_MINUTE)
            / (parameters->N[0] + parameters->N[1]), 10);
    return rand() % (time * 2);
}

CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation) {    
    if (cave_simulation->shared_memory->interrupted)
        return CAVE_SIMULATION_RUN_FAIL;

    cave_simulation->simulation_running = true;

    logger_log(&cave_simulation->logger_interface,
            "Running cave simulation (PID: %d)", getpid());

    setpgid(0, 0);
    signal(SIGUSR1, SIG_IGN);

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
    cave_simulation->shared_memory->processes_starting++;
    cave_simulation->shared_memory->ticket_clerk_pid = fork_res;

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
        logger_log(&cave_simulation->logger_interface, "Guard started (PID: %d)", fork_res);
        cave_simulation->child_processes++;
        cave_simulation->shared_memory->processes_starting++;
    }

    int last_time = cave_simulation->shared_memory->time;
    int to_next_visitor = random_time_between_visitors(
            &cave_simulation->shared_memory->parameters);

    do {
        if (cave_simulation->shared_memory->time - last_time >= to_next_visitor) {
            last_time = cave_simulation->shared_memory->time;
            to_next_visitor = random_time_between_visitors(
                    &cave_simulation->shared_memory->parameters);

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
                cave_simulation->shared_memory->processes_starting++;
            }
        }

        usleep(MILLISECONDS_IN_SECOND);
    } while (!cave_simulation->shared_memory->interrupted && cave_simulation->shared_memory->time
        < calculate_closing_time(&cave_simulation->shared_memory->parameters));

    logger_log(&cave_simulation->logger_interface, "New visitors stopped coming");

    return CAVE_SIMULATION_SUCCESS;
}

void cave_simulation_terminate(CaveSimulation *cave_simulation) {
    cave_simulation->shared_memory->interrupted = true;
}
