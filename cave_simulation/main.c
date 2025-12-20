#include "cave_simulation.h"

CaveSimulation cave_simulation;

int main(void) {
    CaveSimulationRes res = cave_simulation_init(&cave_simulation);
    if (res != CAVE_SIMULATION_SUCCESS)
        return res;

    res = cave_simulation_run(&cave_simulation);

    CaveSimulationRes destroy_res = cave_simulation_destroy(&cave_simulation);

    if (res != CAVE_SIMULATION_SUCCESS)
        return res;
    
    if (destroy_res != CAVE_SIMULATION_SUCCESS)
        return destroy_res;
}
