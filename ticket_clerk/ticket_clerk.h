#pragma once

#include "common.h"
#include "logger_interface.h"
#include <stdbool.h>

// Delay between ticket sales
#define TICKET_CLERK_DELAY 10 //In miliseconds
#define TICKET_COST 26

typedef struct {
    SharedMemory *shared_memory;
    int message_queue;
    int semaphores;
    LoggerInterface logger;
} TicketClerk;

typedef enum {
    TICKET_CLERK_SUCCESS = 0,

    TICKET_CLERK_INIT_FAIL,
    TICKET_CLERK_DESTROY_FAIL,
    TICKET_CLERK_RUN_FAIL
} TicketClerkRes;

TicketClerkRes ticket_clerk_init(TicketClerk *ticket_clerk);
TicketClerkRes ticket_clerk_destroy(TicketClerk *ticket_clerk);
TicketClerkRes ticket_clerk_run(TicketClerk *ticket_clerk);
