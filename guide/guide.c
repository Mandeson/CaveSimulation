#include "guide.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <unistd.h>
#include "common.h"
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
    guide->number = (getpid() == guide->shared_memory->guide2_pid) ? 1 : 0;
    give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

    logger_interface_new(&guide->logger, "Guide");

    return GUIDE_SUCCESS;
}

GuideRes guide_destroy(Guide *guide) {
    if (guide->logger_initialized)
        logger_log(&guide->logger,
                "Guide (PID: %d) destroying", getpid());

    if (detach_shared_memory(guide->shared_memory) == -1)
        return GUIDE_DESTROY_FAIL;

    if (guide->trail_visitors_initialized)
        array_destroy(&guide->trail_visitors);

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

void guide_tour(Guide *guide) {
    logger_log(&guide->logger,
            "Guide of trail %d is starting the tour", guide->number + 1);

    size_t person_space = PIPE_BUF / guide->shared_memory->K;
    void *buffer = malloc(person_space);

    sleep(1000000);

    free(buffer);

    guide->tour_cancelled = false;
}

GuideRes guide_run(Guide *guide) {
    pid_t pid = getpid();
    logger_log(&guide->logger,
            "Running guide (PID: %d, guide nr: %d)", pid, guide->number + 1);
    guide->logger_initialized = true;

    int waiting_before_catwalk_semaphore = (guide->number == 1) ? WAITING_BY_GUIDE2_SEMAPHORE : WAITING_BY_GUIDE1_SEMAPHORE;
    int timeout = (guide->shared_memory->T[guide->number]) * 60;

    bool terminate = false;
    int timeout_counter = 0;
    do {
        int sleep_time = 1;
        usleep(sleep_time * 1000);
        timeout_counter += sleep_time;

        // Go on the tour if the limit of N people is reached or if no new visitors are coming,
        // at least one person is waiting and the timeout is reached
        // if (semaphore_value == 0 || (guide->shared_memory->terminating
        //         && semaphore_value < guide->shared_memory->N[guide->number]
        //         && timeout_counter >= timeout)) {
        //     give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);

        //     set_semaphore(guide->semaphores, trail_semaphore, 0);
        //     timeout_counter = 0;

        //     while (guide->trail_visitors_count <
        //             guide->shared_memory->N[guide->number] - semaphore_value) {
        //         receive_message(guide, 0);
        //     }

        //     guide_tour(guide);
        // } else {
        //     give_semaphore(guide->semaphores, SHARED_MEMORY_SEMAPHORE);
        // }

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
