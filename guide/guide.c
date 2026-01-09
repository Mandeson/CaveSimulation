#include "guide.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"

typedef enum {
    PREPARE,

} GuideState;

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

    take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
    guide->number = guide->shared_memory->guides_count++;
    give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    return GUIDE_SUCCESS;
}

GuideRes guide_destroy(Guide *guide) {
    output_log(guide->semaphores, guide->shared_memory,
            "Destroying guide (PID: %d)", getpid());

    if (detach_shared_memory(guide->shared_memory) == -1)
        return GUIDE_DESTROY_FAIL;

    return GUIDE_SUCCESS;
}

static int receive_message(Guide *guide, bool *terminate, int flags) {
    Message message;
    int res = msgrcv(guide->message_queue, (void *)&message, sizeof(message.mtext), getpid(),
            flags);
    if (res == -1) {
        if (errno != ENOMSG) {
            perror("receive_message: msgrcv");
            return -1;
        }
    } else if (res != sizeof(message.mtext)) {
        output_log(guide->semaphores, guide->shared_memory,
                "receive_message: wrong number of bytes received from message queue");
        return -1;
    } else {
        if (strcmp(message.mtext, "terminate") == 0) {
            *terminate = true;
        } else {
            output_log(guide->semaphores, guide->shared_memory,
                "receive_message: unknown message received from message queue");
        }
    }

    return 0;
}

void open_gate(Guide *guide) {
    int trail_semaphore = (guide->number == 1) ? TRAIL2_SEMAPHORE : TRAIL1_SEMAPHORE;
    set_semaphore(guide->semaphores, trail_semaphore, guide->shared_memory->N[guide->number]);
}

GuideRes guide_run(Guide *guide) {
    pid_t pid = getpid();
    output_log(guide->semaphores, guide->shared_memory,
            "Running guide (PID: %d, guide nr: %d)", pid, guide->number + 1);
    
    size_t person_space = PIPE_BUF / guide->shared_memory->K;
    void *buffer = malloc(person_space);

    open_gate(guide);

    bool terminate = false;
    do {
        usleep(1000);

        take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        volatile int *catwalk_visitors = guide->shared_memory->catwalk_visitors;
        int catwalk_number = (catwalk_visitors[1] > catwalk_visitors[0]) ? 1 : 0;
        bool empty = (catwalk_visitors[0] == 0) && (catwalk_visitors[1] == 0);

        if(empty) {
            give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        } else {
            int catwalk_pipe = guide->shared_memory->catwalk_pipe[catwalk_number][0];
            output_log(guide->semaphores, guide->shared_memory,
                        "R %d %d %d %d", guide->number, catwalk_visitors[0], catwalk_visitors[1], catwalk_number);
            int res = read(catwalk_pipe, buffer, person_space);
            catwalk_visitors[catwalk_number] -= 1;
            output_log(guide->semaphores, guide->shared_memory,
                        "RF %d %d %d %d", guide->number, catwalk_visitors[0], catwalk_visitors[1], catwalk_number);
            if (res == -1) {
                perror("guide_run: read");
                free(buffer);
                give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                return GUIDE_RUN_FAIL;
            }
            if ((size_t)res != person_space) {
                give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                output_log(guide->semaphores, guide->shared_memory,
                        "Guide error: pipe read size mismatch");
            } else {
                VisitorOnCatwalk visitor_on_catwalk;
                memcpy(&visitor_on_catwalk, buffer, sizeof(VisitorOnCatwalk));
                output_log(guide->semaphores, guide->shared_memory,
                        "Guide %d has read PID: %d from the pipe", guide->number, visitor_on_catwalk.pid);

                give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
                
                // Message message = {0};
                // message.mtype = visitor_on_catwalk.pid;
                // msgsnd(guide->message_queue, &message, sizeof(message.mtext), 0);
            }
        }

        receive_message(guide, &terminate, IPC_NOWAIT);
    } while (!terminate);

    free(buffer);

    return GUIDE_SUCCESS;
}
