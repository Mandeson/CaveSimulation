#pragma once

#include "common.h"
#include <sys/ipc.h>

// Maximum delay between new visitors coming to the ticket office
#define CAVE_SIMULATION_MAX_VISITORS_DELAY 20

typedef struct {
    volatile bool terminating;
    int shared_memory_id;
    int message_queue;
    int semaphores;
    int child_processes;

    SharedMemory *shared_memory;
} CaveSimulation;

typedef enum {
    CAVE_SIMULATION_SUCCESS = 0,

    CAVE_SIMULATION_INIT_FAIL,
    CAVE_SIMULATION_DESTROY_FAIL,
    CAVE_SIMULATION_RUN_FAIL
} CaveSimulationRes;

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation);
CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation);
CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation);
void cave_simulation_terminate(CaveSimulation *cave_simulation);
