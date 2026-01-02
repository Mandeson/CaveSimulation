#include "ticket_clerk.h"
#include "common.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <unistd.h>

TicketClerkRes ticket_clerk_init(TicketClerk *ticket_clerk) {
    // Get time for random seed (microseconds)
    struct timeval time;
    gettimeofday(&time, NULL);
    srand(time.tv_usec);

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

static void ticket_clerk_sell_tickets(TicketClerk *ticket_clerk, const VisitorMessage *request) {
    uint8_t trail_nr;
    const VisitorInfo *info = &request->visitor_info;
    if (info->age > 75 || info->children_count > 0) {
        trail_nr = 2;
    } else {
        trail_nr = rand() % 2 + 1;
    }

    int cost = TICKET_COST;
    for (int i = 0; i < info->children_count; i++) {
        if (info->children_ages[i] >= 3)
            cost += TICKET_COST;
    }

    TicketMessage ticket_message;
    ticket_message.trail_nr = trail_nr;
    ticket_message.cost = cost;

    Message message = {0};
    message.mtype = request->pid;
    memcpy(&message.mtext, &ticket_message, sizeof(ticket_message));
    if (msgsnd(ticket_clerk->message_queue, &message, sizeof(message.mtext), 0) == -1)
        perror("ticket_clerk_sell_tickets: msgsnd");
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
                if (strcmp(message.mtext, "terminate") == 0) {
                    terminate = true;
                } else {
                    VisitorMessage visitor_message;
                    memcpy(&visitor_message, message.mtext, sizeof(visitor_message));
                    output_log(ticket_clerk->semaphores, ticket_clerk->shared_memory,
                            "TicketClerk received ticket request from PID: %d, age: %d, "
                            "children: %d and sends the ticket",
                            visitor_message.pid, visitor_message.visitor_info.age,
                            visitor_message.visitor_info.children_count);
                    ticket_clerk_sell_tickets(ticket_clerk, &visitor_message);
                }
            }
        } while (res != -1);

        // Critical section
        take_semaphore(ticket_clerk->semaphores, SHARED_MEMORY_SEMAPHORE);

        if (ticket_clerk->shared_memory->priority_ticket_line_size > 0) {
            give_semaphore(ticket_clerk->semaphores, TICKET_PRIORITY_SEMAPHORE);
            ticket_clerk->shared_memory->priority_ticket_line_size--;
        } else if (ticket_clerk->shared_memory->regular_ticket_line_size > 0) {
            give_semaphore(ticket_clerk->semaphores, TICKET_REGULAR_SEMAPHORE);
            ticket_clerk->shared_memory->regular_ticket_line_size--;
        }
        empty = (ticket_clerk->shared_memory->visitors_count == 0);

        give_semaphore(ticket_clerk->semaphores, SHARED_MEMORY_SEMAPHORE);

        usleep(TICKET_CLERK_DELAY * 1000);
    } while (!terminate || !empty);

    return TICKET_CLERK_SUCCESS;
}
