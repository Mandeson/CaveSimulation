#include "guide.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"
#include "logger_interface.h"
#include "util/array.h"

GuideRes guide_init(Guide *guide) {
    array_create(&guide->trail_visitors, sizeof(int));
    guide->trail_visitors_initialized = true;

    SharedMemory *shared_memory = attach_shared_memory();
    if (shared_memory == NULL)
        return GUIDE_INIT_FAIL;
    guide->shared_memory = shared_memory;
    
    int message_queue = get_message_queue(MESSAGE_QUEUE_ID);
    if (message_queue == -1) {
        if (shared_memory != NULL)
            detach_shared_memory(&shared_memory);
        return GUIDE_INIT_FAIL;
    }
    guide->message_queue = message_queue;

    int semaphores = get_semaphores();
    if (semaphores == -1) {
        if (shared_memory != NULL)
            detach_shared_memory(&shared_memory);
        return GUIDE_INIT_FAIL;
    }
    guide->semaphores = semaphores;

    take_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
    guide->number = (getpid() == guide->shared_memory->guide2_pid) ? 1 : 0;
    give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    char logger_tag[] = "Guide n";
    logger_tag[6] = '1' + guide->number;
    logger_interface_new(&guide->logger, logger_tag, shared_memory);

    return GUIDE_SUCCESS;
}

GuideRes guide_destroy(Guide *guide) {
    // if (guide->logger_initialized)
    //     logger_log(&guide->logger,
    //             "Guide (PID: %d) destroying", getpid());

    if (guide->trail_visitors_initialized)
        array_destroy(&guide->trail_visitors);

    if (guide->shared_memory != NULL
            && detach_shared_memory(&guide->shared_memory) == -1)
        return GUIDE_DESTROY_FAIL;

    return GUIDE_SUCCESS;
}

static int receive_message(Guide *guide) {
    Message message;
    int res = message_queue_receive(guide->message_queue, getpid(), &message, "receive_message", false);
    if (res == MESSAGE_QUEUE_RECEIVE_FAIL) {
        return -1;
    } else if (res != MESSAGE_QUEUE_RECEIVE_NO_MESSAGE) {
        if (strcmp(message.mtext, "terminate") == 0) {
            guide->terminate = true;
        } else {
            logger_log(&guide->logger, "Error: receive_message: unknown message");
        }

        return 1;
    }

    return 0;
}

static void collect_visitors(Guide *guide) {
    size_t person_space = PIPE_BUF / guide->shared_memory->K;
    void *buffer = malloc(person_space);

    size_t visitors_collected = 0;
    while (visitors_collected < guide->trail_visitors.size) {//TOCHANGHE TO trail_visitors_count
        //usleep(1000);

        int catwalk_visitors[2] = {guide->shared_memory->catwalk_visitors[0],
                guide->shared_memory->catwalk_visitors[1]};
        if (catwalk_visitors[0] != 0 || catwalk_visitors[1] != 0) {
            logger_log(&guide->logger, "Collecting a visitor");

            int catwalk_number = (catwalk_visitors[1] > catwalk_visitors[0]) ? 1 : 0;

            while (read(guide->shared_memory->catwalk_pipe[catwalk_number][0], buffer,
                    person_space) == -1) {
                if (errno != EINTR)
                    perror("guide_tour: read (pipe)");
            }

            guide->shared_memory->catwalk_visitors[catwalk_number]--; // TOCHANGE
            visitors_collected++;

            logger_log(&guide->logger, "Collected a visitor");
        }
    }

    free(buffer);
}

static void guide_tour(Guide *guide) {
    logger_log(&guide->logger,
            "Starting the tour with %d visitors", guide->number + 1,
            guide->trail_visitors_count);

    take_semaphore(guide->semaphores, CATWALK_SEMAPHORE);

    for (size_t i = 0; i < guide->trail_visitors.size; i++) {
        int visitor_pid = ((int *)guide->trail_visitors.ptr)[i];
        message_queue_send(guide->message_queue, visitor_pid, NULL, 0, "guide_tour");
    }

    collect_visitors(guide);

    give_semaphore(guide->semaphores, CATWALK_SEMAPHORE);

    logger_log(&guide->logger, "Ending the tour");

    array_clear(&guide->trail_visitors);
    guide->trail_visitors_count = 0;

    guide->tour_cancelled = false;
}

// Returns true if there are enough visitors to start the tour
bool greet_visitors(Guide *guide) {
    take_semaphore(guide->semaphores, guide->waiting_by_guide_semaphore);

    bool enough_visitors = false;

    for (int i = 0; i < VISITORS_WAITING_SIZE; i++) {
        volatile VisitorWaiting *visitor_waiting =
                &guide->shared_memory->visitors_waiting[guide->number][i];
        if (visitor_waiting->pid != 0) {
            if (guide->trail_visitors_count + 1 + visitor_waiting->children_count
                    <= guide->shared_memory->N[guide->number]) {
                guide->trail_visitors_count += 1 + visitor_waiting->children_count;
                int *pid = array_add_empty(&guide->trail_visitors);
                *pid = visitor_waiting->pid;
                logger_log(&guide->logger, "%d greets visitor %d", guide->number + 1, *pid);

                visitor_waiting->pid = 0;
            } else {
                enough_visitors = true;
            }
        }
    }
    
    give_semaphore(guide->semaphores, guide->waiting_by_guide_semaphore);

    // Even if there is no one left waiting, if the number of visitors is equal to the limit,
    // start the tour
    if (guide->trail_visitors_count == guide->shared_memory->N[guide->number])
        enough_visitors = true;

    return enough_visitors;
}

GuideRes guide_run(Guide *guide) {
    pid_t pid = getpid();
    logger_log(&guide->logger, "Running guide (PID: %d)", pid);
    guide->logger_initialized = true;

    guide->waiting_by_guide_semaphore = (guide->number == 1) ? WAITING_BY_GUIDE2_SEMAPHORE
            : WAITING_BY_GUIDE1_SEMAPHORE;
    int timeout = (guide->shared_memory->T[guide->number]) * 60;
    int timeout_counter = 0;
    do {
        if (greet_visitors(guide) || (timeout_counter >= timeout && guide->trail_visitors_count > 0)) {
            guide_tour(guide);
            timeout_counter = 0;
        }

        int sleep_time = 1;
        while (usleep(sleep_time * 1000) == -1) {}
        timeout_counter += sleep_time;

        int res;
        do {
            res = receive_message(guide);
        } while (res == 1);
    } while (!guide->terminate);

    return GUIDE_SUCCESS;
}

void guide_signal(Guide *guide) {
    guide->tour_cancelled = true;
}
