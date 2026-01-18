#pragma once

#include "common.h"
#include "logger.h"
#include <sys/ipc.h>

// Maximum delay between new visitors coming to the ticket office
#define CAVE_SIMULATION_MAX_VISITORS_DELAY 20
#define MAX_PROCESSES 2000

typedef struct {
    bool disable_guard;
    int shared_memory_id;
    int message_queue;
    int semaphores;
    int child_processes;
    int guard_pid;

    Logger logger;
    LoggerInterface logger_interface;

    SharedMemory *shared_memory;
} CaveSimulation;

typedef enum {
    CAVE_SIMULATION_SUCCESS = 0,

    CAVE_SIMULATION_INIT_FAIL,
    CAVE_SIMULATION_DESTROY_FAIL,
    CAVE_SIMULATION_RUN_FAIL
} CaveSimulationRes;

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation, bool log_to_stdout,
        bool skip_start_confirmation, bool disable_guard);
CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation);
CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation);
void cave_simulation_terminate(CaveSimulation *cave_simulation);
