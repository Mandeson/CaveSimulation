#pragma once

#include "clock.h"
#include "common.h"
#include "logger.h"
#include "logger_interface.h"
#include <stdatomic.h>
#include <sys/ipc.h>

#define MAX_PROCESSES 500

typedef struct {
    bool disable_guard;

    int shared_memory_id;
    int message_queue;
    int semaphores;

    int guard_pid;

    Clock clock;
    volatile atomic_bool simulation_running;
    volatile atomic_int child_processes;
    volatile atomic_int child_processes_finished;

    Logger logger;
    LoggerInterface logger_interface;
    pthread_t child_wait_thread;

    SharedMemory *shared_memory;
} CaveSimulation;

typedef enum {
    CAVE_SIMULATION_SUCCESS = 0,

    CAVE_SIMULATION_INIT_FAIL,
    CAVE_SIMULATION_DESTROY_FAIL,
    CAVE_SIMULATION_RUN_FAIL
} CaveSimulationRes;

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation,
        const SimulationParameters *parameters, bool log_to_stdout,
        bool skip_start_confirmation, bool disable_guard);
CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation);
CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation);
void cave_simulation_terminate(CaveSimulation *cave_simulation);
