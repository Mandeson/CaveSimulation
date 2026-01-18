#include "visitor.h"
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"

static void init_parameters(Visitor *visitor) {
    // Get time for random seed (microseconds)
    struct timeval time;
    gettimeofday(&time, NULL);
    srand(time.tv_usec);

    int age = rand() % (80 + 1 - 8) + 8;
    visitor->visitor_info.age = age;
    if (age <= 18) {
        visitor->visitor_info.children_count = 0;
    } else {
        int children_count = rand() % 3;
        visitor->visitor_info.children_count = children_count;
        for (int i = 0; i < children_count; i++) {
            int max_child_age = MIN(7, age - 18);
            visitor->visitor_info.children_ages[i] = rand() % (max_child_age + 1 - 1) + 1;
        }
    }
}

VisitorRes visitor_init(Visitor *visitor) {
    init_parameters(visitor);

    SharedMemory *shared_memory = attach_shared_memory();
    if (shared_memory == NULL)
        return VISITOR_INIT_FAIL;
    visitor->shared_memory = shared_memory;
    
    int message_queue = get_message_queue(MESSAGE_QUEUE_ID);
    if (message_queue == -1) {
        detach_shared_memory(shared_memory);
        return VISITOR_INIT_FAIL;
    }
    visitor->message_queue = message_queue;

    int semaphores = get_semaphores();
    if (semaphores == -1) {
        detach_shared_memory(shared_memory);
        return VISITOR_INIT_FAIL;
    }
    visitor->semaphores = semaphores;

    logger_interface_new(&visitor->logger, "Visitor");

    return VISITOR_SUCCESS;
}

VisitorRes visitor_destroy(Visitor *visitor) {
    if (visitor->shared_memory != NULL && detach_shared_memory(visitor->shared_memory) == -1)
        return VISITOR_DESTROY_FAIL;

    return VISITOR_SUCCESS;
}

VisitorRes visitor_run(Visitor *visitor) {
    pid_t pid = getpid();
    logger_log(&visitor->logger,
            "Running visitor (PID: %d)", pid);

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    visitor->shared_memory->regular_ticket_line_size++;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    take_semaphore(visitor->semaphores, TICKET_REGULAR_SEMAPHORE);

    logger_log(&visitor->logger,
            "Visitor %d enters the ticket office and requests ticket", pid);

    // Request ticket from TicketClerk
    VisitorMessage visitor_message;
    visitor_message.pid = pid;
    visitor_message.visitor_info = visitor->visitor_info;
    if (message_queue_send(visitor->message_queue, visitor->shared_memory->ticket_clerk_pid,
            &visitor_message, sizeof(visitor_message), "visitor_run") == MESSAGE_QUEUE_SEND_FAIL)
        return VISITOR_RUN_FAIL;

    Message message = {0};

    // Receive the ticket
    if (message_queue_receive(visitor->message_queue, pid, &message, "visitor_run", true) == MESSAGE_QUEUE_RECEIVE_FAIL)
        return VISITOR_RUN_FAIL;

    TicketMessage ticket_message;
    memcpy(&ticket_message, message.mtext, sizeof(TicketMessage));

    logger_log(&visitor->logger,
            "Visitor (PID: %d) has received the ticket for trail %d and pays %d golden coins",
            pid, ticket_message.trail_nr, ticket_message.cost);

    logger_log(&visitor->logger,
            "Visitor (PID: %d) approaches the guide of trail %d", pid, ticket_message.trail_nr + 1);

    int children_count = visitor->visitor_info.children_count;
    
    int guide_semaphore = (ticket_message.trail_nr == 1)
            ? WAITING_BY_GUIDE2_SEMAPHORE : WAITING_BY_GUIDE1_SEMAPHORE;
    take_semaphore(visitor->semaphores, guide_semaphore);
    bool found_spot = false;

    // Find a spot for the visitor
    for (int i = 0; i < VISITORS_WAITING_SIZE; i++) {
        volatile VisitorWaiting *visitor_waiting = &visitor->shared_memory->visitors_waiting[ticket_message.trail_nr][i];
        if (visitor_waiting->pid == 0) {
            visitor_waiting->pid = pid;
            visitor_waiting->children_count = children_count;
            found_spot = true;
            break;
        }
    }

    int visitors_count = 0;
    for (int i = 0; i < VISITORS_WAITING_SIZE; i++) {
        if (visitor->shared_memory->visitors_waiting[ticket_message.trail_nr][i].pid != 0)
            visitors_count += visitor->shared_memory->visitors_waiting[ticket_message.trail_nr][i].children_count + 1;
    }

    give_semaphore(visitor->semaphores, guide_semaphore);

    if (found_spot) {
        logger_log(&visitor->logger,
                "Visitor (PID: %d) has found a spot near guide %d, %d visitors waiting",
                pid, ticket_message.trail_nr + 1, visitors_count);
    } else {
        logger_log(&visitor->logger,
                "Error: visitor_run: no spot found near guide %d", ticket_message.trail_nr + 1);
        return VISITOR_RUN_FAIL;
    }

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    visitor->shared_memory->visitors_finished++;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    logger_log(&visitor->logger,
            "Visitor (PID: %d) shutting down", pid);

    return VISITOR_SUCCESS;
}
