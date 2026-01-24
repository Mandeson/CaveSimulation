#include "visitor.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"
#include "logger_interface.h"
#include "util/time.h"

static void init_parameters(Visitor *visitor) {
    // Get time for random seed (microseconds)
    struct timeval time;
    gettimeofday(&time, NULL);
    srand(time.tv_usec);

    int age = rand() % (VISITOR_MAX_AGE + 1 - VISITOR_MIN_AGE) + VISITOR_MIN_AGE;
    visitor->visitor_info.age = age;
    if (age <= 18) {
        visitor->visitor_info.children_count = 0;
    } else {
        int children_count = rand() % 3;
        visitor->visitor_info.children_count = children_count;
        for (int i = 0; i < children_count; i++) {
            int max_child_age = MIN(7, age - 18);
            visitor->visitor_info.children_ages[i] = rand()
                    % (max_child_age + 1 - VISITOR_MIN_AGE) + VISITOR_MIN_AGE;
        }
    }

    if (visitor->visitor_info.age >= VISITOR_TRAIL_1_MAX_AGE
            || visitor->visitor_info.children_count > 0) {
        visitor->visitor_info.trail_nr = 1;
    } else {
        visitor->visitor_info.trail_nr = rand() % 2;
    }

    visitor->visitor_info.second_tour = false;
}

VisitorRes visitor_init(Visitor *visitor) {
    init_parameters(visitor);

    SharedMemory *shared_memory = attach_shared_memory();
    if (shared_memory == NULL)
        return VISITOR_INIT_FAIL;
    visitor->shared_memory = shared_memory;

    visitor->shared_memory->processes_starting--;
    
    int message_queue = get_message_queue(MESSAGE_QUEUE_ID);
    if (message_queue == -1) {
        if (shared_memory != NULL)
            detach_shared_memory(&shared_memory);
        return VISITOR_INIT_FAIL;
    }
    visitor->message_queue = message_queue;

    int semaphores = get_semaphores();
    if (semaphores == -1) {
        if (shared_memory != NULL)
            detach_shared_memory(&shared_memory);
        return VISITOR_INIT_FAIL;
    }
    visitor->semaphores = semaphores;

    char logger_tag[25];
    snprintf(logger_tag, sizeof(logger_tag), "Visitor (PID %d)", getpid());
    logger_interface_new(&visitor->logger, logger_tag, shared_memory);

    if (visitor->shared_memory->interrupted) {
        if (visitor->shared_memory != NULL)
            detach_shared_memory(&visitor->shared_memory);
        return VISITOR_INIT_FAIL;
    }

    return VISITOR_SUCCESS;
}

VisitorRes visitor_destroy(Visitor *visitor) {
    // if (visitor->logger_initialized)
    //     logger_log(&visitor->logger, "Destroying");

    if (visitor->shared_memory != NULL && detach_shared_memory(&visitor->shared_memory) == -1)
        return VISITOR_DESTROY_FAIL;

    return VISITOR_SUCCESS;
}

static int move_through_catwalk(Visitor *visitor) {
    int catwalk_number = (visitor->shared_memory->catwalk_visitors[1]
            < visitor->shared_memory->catwalk_visitors[0]) ? 1 : 0;
    visitor->shared_memory->catwalk_visitors[catwalk_number]
            += 1 + visitor->visitor_info.children_count;

    logger_log(&visitor->logger, "Entering the catwalk %d with %d children",
            catwalk_number + 1, visitor->visitor_info.children_count);

    size_t person_space = PIPE_BUF / visitor->shared_memory->parameters.K;
    void *buffer = malloc(person_space);
    if (buffer == NULL) {
        logger_log(&visitor->logger, "Error: move_through_catwalk: malloc failed");
        return -1;
    }

    // Spend time walking through the catwalk
    usleep(visitor->shared_memory->parameters.K * MILLISECONDS_IN_SECOND);

    for (int i = 0; i < 1 + visitor->visitor_info.children_count; i++) {
        if (write(visitor->shared_memory->catwalk_pipe[catwalk_number][1], buffer, person_space)
                == -1) {
            perror("visitor_run: write (pipe)");
            visitor->shared_memory->catwalk_visitors[catwalk_number]
                    -= 1 + visitor->visitor_info.children_count;
            free(buffer);
            return -1;
        }
    }

    logger_log(&visitor->logger, "Leaving the catwalk with %d children",
            visitor->visitor_info.children_count);

    free(buffer);

    return 0;
}

static int visitor_go(Visitor *visitor) {
    pid_t pid = getpid();

    // Request ticket from TicketClerk
    VisitorMessage visitor_message;
    visitor_message.pid = pid;
    visitor_message.visitor_info = visitor->visitor_info;
    if (message_queue_send(visitor->message_queue, visitor->shared_memory->ticket_clerk_pid,
            &visitor_message, sizeof(visitor_message), "visitor_run") == MESSAGE_QUEUE_SEND_FAIL)
        return -1;

    Message message = {0};

    // Receive the ticket
    if (message_queue_receive(visitor->message_queue, pid, &message, "visitor_run - ticket", true)
            == MESSAGE_QUEUE_RECEIVE_FAIL)
        return -1;

    if (strcmp(message.mtext, "no-tickets") == 0) {
        logger_log(&visitor->logger, "Received no tickets");
        return -1;
    }

    TicketMessage ticket_message;
    memcpy(&ticket_message, message.mtext, sizeof(TicketMessage));

    logger_log(&visitor->logger,
            "Received the ticket for trail %d and pays %d golden coins",
            ticket_message.trail_nr + 1, ticket_message.cost);

    if (message_queue_receive(visitor->message_queue, pid, &message, "visitor_run - catwalk", true)
            != MESSAGE_QUEUE_RECEIVE_SUCCESS)
        return -1;
    
    if (move_through_catwalk(visitor) == -1)
        return -1;

    // Wait for the guide to start the tour
    if (message_queue_receive(visitor->message_queue, pid, &message, "visitor_run - tour", true)
            != MESSAGE_QUEUE_RECEIVE_SUCCESS)
        return -1;
    
    if (strcmp(message.mtext, "tour-cancelled") == 0) {
        logger_log(&visitor->logger, "Tour cancelled");
    } else {
        logger_log(&visitor->logger, "Started the tour");

        usleep(visitor->shared_memory->parameters.T[ticket_message.trail_nr] * SECONDS_IN_MINUTE
                * MILLISECONDS_IN_SECOND);

        logger_log(&visitor->logger, "Finished the tour");
    }

    if (move_through_catwalk(visitor) == -1)
        return -1;

    if (message_queue_receive(visitor->message_queue, pid, &message, "visitor_run - catwalk (out)", true)
            != MESSAGE_QUEUE_RECEIVE_SUCCESS)
        return -1;

    return 0;
}

VisitorRes visitor_run(Visitor *visitor) {
    logger_log(&visitor->logger, "Running visitor");
    visitor->logger_initialized = true;

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    visitor->shared_memory->regular_ticket_line_size++;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    take_semaphore(visitor->semaphores, TICKET_REGULAR_SEMAPHORE);

    logger_log(&visitor->logger, "Entered the ticket office and is requesting a ticket");

    if (visitor_go(visitor) == -1)
        return VISITOR_RUN_FAIL;

    // 10% of visitors want to go on the other trail on the same day
    // However, only visitors younger than VISITOR_TRAIL_1_MAX_AGE
    // and without children can go on trail 1
    if (rand() % 10 == 0 && visitor->visitor_info.age < VISITOR_TRAIL_1_MAX_AGE
            && visitor->visitor_info.children_count == 0) {
        visitor->visitor_info.trail_nr = !visitor->visitor_info.trail_nr; // Pick the other trail
        visitor->visitor_info.second_tour = true;

        take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
        visitor->shared_memory->priority_ticket_line_size++;
        give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

        take_semaphore(visitor->semaphores, TICKET_PRIORITY_SEMAPHORE);

        logger_log(&visitor->logger, "Decided to go on the other trail, entered the ticket "
            "office again, skipping the line and is requesting a ticket");

        if (visitor_go(visitor) == -1)
            return VISITOR_RUN_FAIL;
    }

    return VISITOR_SUCCESS;
}
