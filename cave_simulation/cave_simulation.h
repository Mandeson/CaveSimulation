#pragma once

#include "common.h"
#include <sys/ipc.h>

typedef struct {
    int shared_memory_id;
    int message_queue;
    int semaphores;

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
