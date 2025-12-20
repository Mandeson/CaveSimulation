#include <stdio.h>
#include <sys/msg.h>
#include <unistd.h>
#include "cave_simulation.h"
#include "common.h"

static const char *ticket_office_queue_creation_error = "Ticket office queue creation error";

CaveSimulationRes cave_simulation_init(CaveSimulation *cave_simulation) {
    key_t ticket_office_queue_key = ftok(".", TICKET_OFFICE_QUEUE_ID);
    if (ticket_office_queue_key == -1) {
        perror(ticket_office_queue_creation_error);
        return CAVE_SIMULATION_INIT_FAIL;
    }

    int ticket_office_queue = msgget(ticket_office_queue_key, IPC_CREAT | IPC_EXCL | 200);
    if (ticket_office_queue == -1) {
        perror(ticket_office_queue_creation_error);
        return CAVE_SIMULATION_INIT_FAIL;
    }
    cave_simulation->ticket_office_queue = ticket_office_queue;

    return CAVE_SIMULATION_SUCCESS;
}

CaveSimulationRes cave_simulation_destroy(CaveSimulation *cave_simulation) {
    msgctl(cave_simulation->ticket_office_queue, IPC_RMID, NULL);

    return CAVE_SIMULATION_SUCCESS;
}

CaveSimulationRes cave_simulation_run(CaveSimulation *cave_simulation) {
    return CAVE_SIMULATION_SUCCESS;
}
