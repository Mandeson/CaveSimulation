#include "ticket_clerk.h"
#include "common.h"
#include "util/time.h"
#include <linux/limits.h>
#include <stdint.h>
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

    ticket_clerk->shared_memory->processes_starting--;

    int message_queue = get_message_queue(MESSAGE_QUEUE_ID);
    if (message_queue == -1) {
        if (shared_memory != NULL)
            detach_shared_memory(&shared_memory);
        return TICKET_CLERK_INIT_FAIL;
    }
    ticket_clerk->message_queue = message_queue;

    int semaphores = get_semaphores();
    if (semaphores == -1) {
        if (shared_memory != NULL)
            detach_shared_memory(&shared_memory);
        return TICKET_CLERK_INIT_FAIL;
    }
    ticket_clerk->semaphores = semaphores;

    logger_interface_new(&ticket_clerk->logger, "TicketClerk", shared_memory);
    ticket_clerk->logger_initialized = true;

    return TICKET_CLERK_SUCCESS;
}

TicketClerkRes ticket_clerk_destroy(TicketClerk *ticket_clerk) {
    // if (ticket_clerk->logger_initialized)
    //     logger_log(&ticket_clerk->logger,
    //             "Destroying ticket clerk (PID: %d)", getpid());

    if (ticket_clerk->shared_memory != NULL
            && detach_shared_memory(&ticket_clerk->shared_memory) == -1)
        return TICKET_CLERK_DESTROY_FAIL;

    return TICKET_CLERK_SUCCESS;
}

static int ticket_clerk_add_visitor(TicketClerk *ticket_clerk, const VisitorMessage *request, int trail_nr) {
    int children_count = request->visitor_info.children_count;
    
    int guide_semaphore = (trail_nr == 1)
            ? WAITING_BY_GUIDE2_SEMAPHORE : WAITING_BY_GUIDE1_SEMAPHORE;
    take_semaphore(ticket_clerk->semaphores, guide_semaphore);
    bool found_spot = false;

    // Find a spot for the visitor
    for (int i = 0; i < VISITORS_WAITING_SIZE; i++) {
        volatile VisitorWaiting *visitor_waiting =
                &ticket_clerk->shared_memory->visitors_waiting[trail_nr][i];
        if (visitor_waiting->pid == 0) {
            visitor_waiting->pid = request->pid;
            visitor_waiting->children_count = children_count;
            found_spot = true;
            break;
        }
    }

    int visitors_count = 0;
    for (int i = 0; i < VISITORS_WAITING_SIZE; i++) {
        if (ticket_clerk->shared_memory->visitors_waiting[trail_nr][i].pid != 0) {
            visitors_count += ticket_clerk->shared_memory
                ->visitors_waiting[trail_nr][i].children_count + 1;
        }
    }

    give_semaphore(ticket_clerk->semaphores, guide_semaphore);

    if (found_spot) {
        logger_log(&ticket_clerk->logger,
                "Visitor (PID: %d) has found a spot near guide %d, %d visitors waiting",
                request->pid, trail_nr + 1, visitors_count);
    } else {
        logger_log(&ticket_clerk->logger,
                "Error: ticket_clerk_add_visitor: no spot found near guide %d", trail_nr + 1);
        return -1;
    }

    return 0;
}

static void ticket_clerk_sell_tickets(TicketClerk *ticket_clerk, const VisitorMessage *request) {
    const VisitorInfo *info = &request->visitor_info;

    if (ticket_clerk->shared_memory->time
            < calculate_closing_time(&ticket_clerk->shared_memory->parameters)
            && ticket_clerk_add_visitor(ticket_clerk, request, info->trail_nr) == 0) {
        int cost = TICKET_COST;
        for (int i = 0; i < info->children_count; i++) {
            if (info->children_ages[i] >= 3)
                cost += TICKET_COST;
        }

        TicketMessage ticket_message;
        ticket_message.trail_nr = info->trail_nr;
        
        if (info->second_tour) {
            ticket_message.cost = cost / 2; // 50% discount for second tour
        } else {
            ticket_message.cost = cost;
        }

        message_queue_send(ticket_clerk->message_queue, request->pid, &ticket_message,
                sizeof(ticket_message), "ticket_clerk_sell_tickets");
    } else {
        const char *message = "no-tickets";
        message_queue_send(ticket_clerk->message_queue, request->pid, message,
                strlen(message) + 1, "ticket_clerk_sell_tickets");
    }
}

TicketClerkRes ticket_clerk_run(TicketClerk *ticket_clerk) {
    pid_t pid = getpid();
    
    logger_log(&ticket_clerk->logger,
            "Running ticket clerk (PID: %d)", pid);

    char closing_time_str[9];
    time_to_string(closing_time_str, time_from_seconds(
            calculate_closing_time(&ticket_clerk->shared_memory->parameters),
            &ticket_clerk->shared_memory->parameters));
    logger_log(&ticket_clerk->logger,
            "Will stop selling tickets at: %s", closing_time_str);

    bool terminate = false;
    do {
        usleep(TICKET_CLERK_DELAY * MILLISECONDS_IN_SECOND);
        
        int res;
        do {
            Message message;
            res = message_queue_receive(ticket_clerk->message_queue, pid, &message, "ticket_clerk_run", false);
            if (res == MESSAGE_QUEUE_RECEIVE_FAIL) {
                return TICKET_CLERK_RUN_FAIL;
            } else if (res != MESSAGE_QUEUE_RECEIVE_NO_MESSAGE) {
                if (strcmp(message.mtext, "terminate") == 0) {
                    terminate = true;
                } else {
                    VisitorMessage visitor_message;
                    memcpy(&visitor_message, message.mtext, sizeof(visitor_message));
                    logger_log(&ticket_clerk->logger,
                            "Received ticket request from PID: %d, age: %d, "
                            "children: %d",
                            visitor_message.pid, visitor_message.visitor_info.age,
                            visitor_message.visitor_info.children_count);

                    ticket_clerk_sell_tickets(ticket_clerk, &visitor_message);
                }
            }
        } while (res != MESSAGE_QUEUE_RECEIVE_NO_MESSAGE);

        take_semaphore(ticket_clerk->semaphores, WAITING_BY_GUIDE1_SEMAPHORE);
        int visitors_count1 = 0;
        for (int i = 0; i < VISITORS_WAITING_SIZE; i++) {
            if (ticket_clerk->shared_memory->visitors_waiting[0][i].pid != 0)
                visitors_count1 += ticket_clerk->shared_memory->visitors_waiting[0][i].children_count + 1;
        }
        give_semaphore(ticket_clerk->semaphores, WAITING_BY_GUIDE1_SEMAPHORE);

        take_semaphore(ticket_clerk->semaphores, WAITING_BY_GUIDE2_SEMAPHORE);
        int visitors_count2 = 0;
        for (int i = 0; i < VISITORS_WAITING_SIZE; i++) {
            if (ticket_clerk->shared_memory->visitors_waiting[1][i].pid != 0)
                visitors_count2 += ticket_clerk->shared_memory->visitors_waiting[1][i].children_count + 1;
        }
        give_semaphore(ticket_clerk->semaphores, WAITING_BY_GUIDE2_SEMAPHORE);

        if (visitors_count1 < ticket_clerk->shared_memory->parameters.N[0]
                && visitors_count2 < ticket_clerk->shared_memory->parameters.N[1]) {
            take_semaphore(ticket_clerk->semaphores, SHARED_MEMORY_SEMAPHORE);

            if (ticket_clerk->shared_memory->priority_ticket_line_size > 0) {
                give_semaphore(ticket_clerk->semaphores, TICKET_PRIORITY_SEMAPHORE);
                ticket_clerk->shared_memory->priority_ticket_line_size--;
            } else if (ticket_clerk->shared_memory->regular_ticket_line_size > 0) {
                give_semaphore(ticket_clerk->semaphores, TICKET_REGULAR_SEMAPHORE);
                ticket_clerk->shared_memory->regular_ticket_line_size--;
            }

            give_semaphore(ticket_clerk->semaphores, SHARED_MEMORY_SEMAPHORE);
        }
    } while (!terminate);

    return TICKET_CLERK_SUCCESS;
}
