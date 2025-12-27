#include "visitor.h"
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
    output_log(visitor->semaphores, visitor->shared_memory,
            "Destroying visitor (PID: %d)", getpid());

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    visitor->shared_memory->visitors_finished++;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    if (detach_shared_memory(visitor->shared_memory) == -1)
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

    take_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);
    bool terminating = visitor->shared_memory->terminating;
    give_semaphore(visitor->semaphores, SHARED_MEMORY_SEMAPHORE);

    if (terminating)
        return VISITOR_RUN_FAIL;

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
    
    return VISITOR_SUCCESS;
}
