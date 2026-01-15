#include "cave_simulation.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>

CaveSimulation cave_simulation;

void sigint_handler(int);

int main(int argsc, char *argv[]) {
    signal(SIGINT, sigint_handler);

    bool log_to_stdout = false;
    bool skip_start_confirmation = false;
    for (int i = 1; i < argsc; i++) {
        if (strcmp(argv[i], "--log-to-stdout") == 0) {
            log_to_stdout = true;
        } else if (strcmp(argv[i], "--skip-start-confirmation") == 0) {
            skip_start_confirmation = true;
        }
    }

    CaveSimulationRes res = cave_simulation_init(&cave_simulation, log_to_stdout,
            skip_start_confirmation);
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
