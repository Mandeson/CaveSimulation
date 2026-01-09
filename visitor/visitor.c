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
    
    int message_queue = get_message_queue();
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

    return VISITOR_SUCCESS;
}

VisitorRes visitor_destroy(Visitor *visitor) {
    if (visitor->shared_memory != NULL && detach_shared_memory(visitor->shared_memory) == -1)
        return VISITOR_DESTROY_FAIL;

    return VISITOR_SUCCESS;
}

VisitorRes visitor_run(Visitor *visitor) {
    pid_t pid = getpid();
    output_log(visitor->semaphores, visitor->shared_memory,
            "Running visitor (PID: %d)", pid);

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    visitor->shared_memory->regular_ticket_line_size++;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    take_semaphore(visitor->semaphores, TICKET_REGULAR_SEMAPHORE);

    output_log(visitor->semaphores, visitor->shared_memory,
            "Visitor %d enters the ticket office and requests ticket", pid);

    // Request ticket from TicketClerk
    VisitorMessage visitor_message;
    visitor_message.pid = pid;
    visitor_message.visitor_info = visitor->visitor_info;

    Message message = {0};
    message.mtype = visitor->shared_memory->ticket_clerk_pid;
    memcpy(message.mtext, (const void *)&visitor_message, sizeof(visitor_message));
    if (msgsnd(visitor->message_queue, &message, sizeof(message.mtext), 0) == -1) {
        perror("visitor_run: msgsnd");
        return VISITOR_RUN_FAIL;
    }

    // Receive the ticket
    int res = msgrcv(visitor->message_queue, &message, sizeof(message.mtext), pid, 0);
    if (res == -1) {
        perror("visitor_run: msgrcv");
        return VISITOR_RUN_FAIL;
    } else if (res != sizeof(message.mtext)) {
        output_log(visitor->semaphores, visitor->shared_memory,
                "visitor_run: wrong number of bytes received from message queue");
        return VISITOR_RUN_FAIL;
    }

    TicketMessage ticket_message;
    memcpy(&ticket_message, message.mtext, sizeof(TicketMessage));

    output_log(visitor->semaphores, visitor->shared_memory,
            "Visitor (PID: %d) has received the ticket for trail %d and pays %d golden coins",
            pid, ticket_message.trail_nr, ticket_message.cost);

    int children_count = visitor->visitor_info.children_count;

    int trail_semaphore = (ticket_message.trail_nr == 2) ? TRAIL2_SEMAPHORE : TRAIL1_SEMAPHORE;
    take_semaphore_n(visitor->semaphores, trail_semaphore, 1 + children_count);

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    volatile int *catwalk_visitors = visitor->shared_memory->catwalk_visitors;
    int catwalk_number = (catwalk_visitors[1] < catwalk_visitors[0]) ? 1 : 0;
    catwalk_visitors[catwalk_number] += 1 + children_count;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    int catwalk_pipe = visitor->shared_memory->catwalk_pipe[catwalk_number][1];

    output_log(visitor->semaphores, visitor->shared_memory,
            "Visitor (PID: %d) trying to enter catwalk %d", pid, catwalk_number);

    size_t person_space = PIPE_BUF / visitor->shared_memory->K;
    size_t space = person_space * (1 + children_count);
    void *buffer = malloc(space);
    memset(buffer, 0, space);
    
    VisitorOnCatwalk on_catwalk;
    on_catwalk.pid = pid;
    on_catwalk.trail_nr = ticket_message.trail_nr;
    memcpy(buffer, &on_catwalk, sizeof(VisitorOnCatwalk));

    // Wait for a time proportional to the length of the catwalk to arrive at the other end
    usleep(visitor->shared_memory->K * 1000);

    output_log(visitor->semaphores, visitor->shared_memory,
            "W %d %d %d", catwalk_visitors[0], catwalk_visitors[1], catwalk_number);
    res = write(catwalk_pipe, buffer, space);
    if (res == -1) {
        perror("visitor_run: write");
        free(buffer);
        return VISITOR_RUN_FAIL;
    }
    output_log(visitor->semaphores, visitor->shared_memory,
            "WF %d %d %d", catwalk_visitors[0], catwalk_visitors[1], catwalk_number);

    free(buffer);

    // Wait for entering the catwalk
    // res = msgrcv(visitor->message_queue, &message, sizeof(message.mtext), pid, 0);
    // if (res == -1) {
    //     perror("visitor_run: msgrcv");
    //     return VISITOR_RUN_FAIL;
    // } else if (res != sizeof(message.mtext)) {
    //     output_log(visitor->semaphores, visitor->shared_memory,
    //             "visitor_run: wrong number of bytes received from message queue");
    //     return VISITOR_RUN_FAIL;
    // }

    return VISITOR_SUCCESS;
}
