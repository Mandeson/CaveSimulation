#include "ticket_clerk.h"
#include "common.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <unistd.h>

TicketClerkRes ticket_clerk_init(TicketClerk *ticket_clerk) {
    SharedMemory *shared_memory = attach_shared_memory();
    if (shared_memory == NULL)
        return TICKET_CLERK_INIT_FAIL;
    ticket_clerk->shared_memory = shared_memory;

    int message_queue = get_message_queue();
    if (message_queue == -1) {
        detach_shared_memory(shared_memory);
        return TICKET_CLERK_INIT_FAIL;
    }
    ticket_clerk->message_queue = message_queue;

    int semaphores = get_semaphores();
    if (semaphores == -1) {
        detach_shared_memory(shared_memory);
        return TICKET_CLERK_INIT_FAIL;
    }
    ticket_clerk->semaphores = semaphores;

    return TICKET_CLERK_SUCCESS;
}

TicketClerkRes ticket_clerk_destroy(TicketClerk *ticket_clerk) {
    if (detach_shared_memory(ticket_clerk->shared_memory) == -1)
        return TICKET_CLERK_DESTROY_FAIL;

    return TICKET_CLERK_SUCCESS;
}

TicketClerkRes ticket_clerk_run(TicketClerk *ticket_clerk) {
    output_log(ticket_clerk->semaphores, ticket_clerk->shared_memory,
            "Running ticket clerk (PID: %d)", getpid());

    output_log(ticket_clerk->semaphores, ticket_clerk->shared_memory,
            "Destroying ticket clerk (PID: %d)", getpid());

    return TICKET_CLERK_SUCCESS;
}
