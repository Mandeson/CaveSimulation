#include "cave_simulation.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_COUNT 1

const SimulationParameters tests[TEST_COUNT] = {
    {
        .Tp = 11,
        .Tk = 18,
        .N = {5, 5},
        .T = {1, 1},
        .K = 3
    }
};

CaveSimulation cave_simulation;

void sigint_handler(int);

int main(int argsc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    signal(SIGUSR1, sigint_handler);

    bool log_to_stdout = false;
    bool skip_start_confirmation = false;
    bool disable_guard = false;
    for (int i = 1; i < argsc; i++) {
        if (strcmp(argv[i], "--log-to-stdout") == 0) {
            log_to_stdout = true;
        } else if (strcmp(argv[i], "--skip-start-confirmation") == 0) {
            skip_start_confirmation = true;
        } else if (strcmp(argv[i], "--disable-guard") == 0) {
            disable_guard = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--log-to-stdout] [--skip-start-confirmation] [--disable-guard]\n", argv[0]);
            return 0;
        } else {
            printf("Unknown command line option: %s\n", argv[i]);
            return -1;
        }
    }

    for (int i = 0; i < TEST_COUNT; i++) {
        CaveSimulationRes res = cave_simulation_init(&cave_simulation, &tests[i], log_to_stdout,
                skip_start_confirmation, disable_guard);
        if (res != CAVE_SIMULATION_SUCCESS)
            return res;

        res = cave_simulation_run(&cave_simulation);

        CaveSimulationRes destroy_res = cave_simulation_destroy(&cave_simulation);

        if (res != CAVE_SIMULATION_SUCCESS)
            return res;
        
        if (destroy_res != CAVE_SIMULATION_SUCCESS)
            return destroy_res;
    }

    return 0;
}

void sigint_handler(int sig) {
    (void)sig; // Unused parameter
    cave_simulation_terminate(&cave_simulation);
}
