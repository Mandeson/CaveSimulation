#include "ticket_clerk.h"
#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
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
    output_log(ticket_clerk->semaphores, ticket_clerk->shared_memory,
            "Destroying ticket clerk (PID: %d)", getpid());

    if (detach_shared_memory(ticket_clerk->shared_memory) == -1)
        return TICKET_CLERK_DESTROY_FAIL;

    return TICKET_CLERK_SUCCESS;
}

TicketClerkRes ticket_clerk_run(TicketClerk *ticket_clerk) {
    pid_t pid = getpid();
    
    output_log(ticket_clerk->semaphores, ticket_clerk->shared_memory,
            "Running ticket clerk (PID: %d)", pid);

    bool terminate = false;
    bool empty;
    do {
        int res;
        do {
            Message message;
            res = msgrcv(ticket_clerk->message_queue, (void *)&message, sizeof(message.mtext), pid,
                    IPC_NOWAIT);
            if (res == -1) {
                if (errno != ENOMSG) {
                    perror("ticket_clerk_run: msgrcv");
                    return TICKET_CLERK_RUN_FAIL;
                }
            } else if (res != sizeof(message.mtext)) {
                output_log(ticket_clerk->semaphores, ticket_clerk->shared_memory,
                        "ticket_clerk_run: wrong number of bytes received from message queue");
                return TICKET_CLERK_RUN_FAIL;
            } else {
                if (strcmp(message.mtext, "terminate") == 0)
                    terminate = true;
            }
        } while (res != -1);

        // Critical section
        take_semaphore(ticket_clerk->semaphores, SHARED_MEMORY_SEMAPHORE);

        if (ticket_clerk->shared_memory->priority_ticket_line_size > 0) {
            give_semaphore(ticket_clerk->semaphores, TICKET_PRIORITY_SEMAPHORE);
            ticket_clerk->shared_memory->visitors_approaching--;
        } else if (ticket_clerk->shared_memory->regular_ticket_line_size > 0) {
            give_semaphore(ticket_clerk->semaphores, TICKET_REGULAR_SEMAPHORE);
            ticket_clerk->shared_memory->visitors_approaching--;
        }
        empty = (ticket_clerk->shared_memory->visitors_approaching == 0);

        give_semaphore(ticket_clerk->semaphores, SHARED_MEMORY_SEMAPHORE);

        usleep(TICKET_CLERK_DELAY * 1000);
    } while (!terminate || !empty);

    return TICKET_CLERK_SUCCESS;
}
