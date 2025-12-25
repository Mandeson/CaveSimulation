#include "visitor.h"
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include "common.h"

static void init_parameters(Visitor *visitor) {
    // Get time for random seed (microseconds)
    struct timeval time;
    gettimeofday(&time, NULL);
    srand(time.tv_usec);

    visitor->age = rand() % (80 - 1) + 1;
    visitor->has_child = false;
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
    return VISITOR_SUCCESS;
}

VisitorRes visitor_run(Visitor *visitor) {
    return VISITOR_SUCCESS;
}
