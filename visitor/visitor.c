#include "visitor.h"
#include <errno.h>
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

    // int children_count = visitor->visitor_info.children_count;

    // take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    // int *catwalk1_visitors = &visitor->shared_memory->catwalk1_visitors;
    // int *catwalk2_visitors = &visitor->shared_memory->catwalk2_visitors;

    // int catwalk_number = 1;
    // if (*catwalk2_visitors < *catwalk1_visitors)
    //     catwalk_number = 2;

    // int *catwalk_visitors = (catwalk_number == 1) ? catwalk1_visitors : catwalk2_visitors;
    // *catwalk_visitors += 1 + children_count;
    // give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    // output_log(visitor->semaphores, visitor->shared_memory, "%d %d", visitor->shared_memory->catwalk1_visitors, visitor->shared_memory->catwalk2_visitors);

    // int catwalk_pipe = (catwalk_number == 1) ? visitor->shared_memory->catwalk1_pipe[1]
    //         : visitor->shared_memory->catwalk2_pipe[1];

    // output_log(visitor->semaphores, visitor->shared_memory,
    //         "Visitor (PID: %d) trying to enter catwalk %d", pid, catwalk_number);

    // size_t person_space = PIPE_BUF / visitor->shared_memory->K;
    // size_t space = person_space * (1 + children_count);
    // void *buffer = malloc(space);
    // memset(buffer, 0, space);
    
    // VisitorOnCatwalk on_catwalk;
    // on_catwalk.pid = pid;
    // on_catwalk.children_count = children_count;
    // memcpy(buffer, &on_catwalk, sizeof(VisitorOnCatwalk));

    // res = write(catwalk_pipe, buffer, space);
    // if (res == -1) {
    //     // EPIPE error is received when the simulation is interrupted.
    //     // We do not need to print this error, only safely terminate the process.
    //     if (errno != EPIPE)
    //         perror("visitor_run: write");
    //     free(buffer);
    //     return VISITOR_RUN_FAIL;
    // }

    // free(buffer);

    res = msgrcv(visitor->message_queue, &message, sizeof(message.mtext), pid, 0);
    if (res == -1) {
        perror("visitor_run: msgrcv");
        return VISITOR_RUN_FAIL;
    } else if (res != sizeof(message.mtext)) {
        output_log(visitor->semaphores, visitor->shared_memory,
                "visitor_run: wrong number of bytes received from message queue");
        return VISITOR_RUN_FAIL;
    }


    output_log(visitor->semaphores, visitor->shared_memory,
            "Visitor (PID: %d) finishes", getpid());

    return VISITOR_SUCCESS;
}
