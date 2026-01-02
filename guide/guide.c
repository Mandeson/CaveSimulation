#include "guide.h"
#include <linux/limits.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"

GuideRes guide_init(Guide *guide) {
    SharedMemory *shared_memory = attach_shared_memory();
    if (shared_memory == NULL)
        return GUIDE_INIT_FAIL;
    guide->shared_memory = shared_memory;
    
    int message_queue = get_message_queue();
    if (message_queue == -1) {
        detach_shared_memory(shared_memory);
        return GUIDE_INIT_FAIL;
    }
    guide->message_queue = message_queue;

    int semaphores = get_semaphores();
    if (semaphores == -1) {
        detach_shared_memory(shared_memory);
        return GUIDE_INIT_FAIL;
    }
    guide->semaphores = semaphores;

    return GUIDE_SUCCESS;
}

GuideRes guide_destroy(Guide *guide) {
    output_log(guide->semaphores, guide->shared_memory,
            "Destroying guide (PID: %d)", getpid());

    if (detach_shared_memory(guide->shared_memory) == -1)
        return GUIDE_DESTROY_FAIL;

    return GUIDE_SUCCESS;
}

GuideRes guide_run(Guide *guide) {
    pid_t pid = getpid();
    output_log(guide->semaphores, guide->shared_memory,
            "Running guide (PID: %d)", pid);

    return GUIDE_SUCCESS;
}
