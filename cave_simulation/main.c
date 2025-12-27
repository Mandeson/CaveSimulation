#include "cave_simulation.h"
#include <signal.h>
#include <unistd.h>

CaveSimulation cave_simulation;

void sigint_handler(int);

int main(void) {
    signal(SIGINT, sigint_handler);

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

void sigint_handler(int sig) {
    (void)sig; // Unused parameter
    cave_simulation_terminate(&cave_simulation);
}
