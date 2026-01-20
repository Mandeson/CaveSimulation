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

    char logger_tag[25];
    snprintf(logger_tag, sizeof(logger_tag), "Visitor (PID %d)", getpid());
    logger_interface_new(&visitor->logger, logger_tag, shared_memory);

    if (visitor->shared_memory->interrupted)
        return VISITOR_INIT_FAIL;

    return VISITOR_SUCCESS;
}

VisitorRes visitor_destroy(Visitor *visitor) {
    // if (visitor->logger_initialized)
    //     logger_log(&visitor->logger, "Destroying");

    if (visitor->shared_memory != NULL && detach_shared_memory(visitor->shared_memory) == -1)
        return VISITOR_DESTROY_FAIL;

    return VISITOR_SUCCESS;
}

VisitorRes visitor_run(Visitor *visitor) {
    pid_t pid = getpid();
    logger_log(&visitor->logger, "Running visitor");
    visitor->logger_initialized = true;

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    visitor->shared_memory->regular_ticket_line_size++;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    take_semaphore(visitor->semaphores, TICKET_REGULAR_SEMAPHORE);

    logger_log(&visitor->logger, "Entering the ticket office and requesting ticket");

    // Request ticket from TicketClerk
    VisitorMessage visitor_message;
    visitor_message.pid = pid;
    visitor_message.visitor_info = visitor->visitor_info;
    if (message_queue_send(visitor->message_queue, visitor->shared_memory->ticket_clerk_pid,
            &visitor_message, sizeof(visitor_message), "visitor_run") == MESSAGE_QUEUE_SEND_FAIL)
        return VISITOR_RUN_FAIL;

    Message message = {0};

    // Receive the ticket
    if (message_queue_receive(visitor->message_queue, pid, &message, "visitor_run", true)
            == MESSAGE_QUEUE_RECEIVE_FAIL)
        return VISITOR_RUN_FAIL;

    if (strcmp(message.mtext, "no-tickets") == 0) {
        logger_log(&visitor->logger, "Received no tickets");
        return VISITOR_RUN_FAIL;
    }

    TicketMessage ticket_message;
    memcpy(&ticket_message, message.mtext, sizeof(TicketMessage));

    logger_log(&visitor->logger,
            "Received the ticket for trail %d and pays %d golden coins",
            ticket_message.trail_nr + 1, ticket_message.cost);

    // Wait for the tour to start
    if (message_queue_receive(visitor->message_queue, pid, &message, "visitor_run", true)
            != MESSAGE_QUEUE_RECEIVE_SUCCESS)
        return VISITOR_RUN_FAIL;

    logger_log(&visitor->logger, "Started the tour");
    
    int catwalk_number = (visitor->shared_memory->catwalk_visitors[1]
            < visitor->shared_memory->catwalk_visitors[0]) ? 1 : 0;
    visitor->shared_memory->catwalk_visitors[catwalk_number]++; // TOCHANGE

    logger_log(&visitor->logger, "Entering the catwalk %d", catwalk_number + 1);

    size_t person_space = PIPE_BUF / visitor->shared_memory->K;
    void *buffer = malloc(person_space);

    // Spend time walking through the catwalk
    usleep(visitor->shared_memory->K * 1000);

    if (write(visitor->shared_memory->catwalk_pipe[catwalk_number][1], buffer, person_space)
            == -1) {
        perror("visitor_run: write (pipe)");
        visitor->shared_memory->catwalk_visitors[catwalk_number]--;
        free(buffer);
        return VISITOR_RUN_FAIL;
    }

    free(buffer);

    logger_log(&visitor->logger, "Finished the tour");

    return VISITOR_SUCCESS;
}
