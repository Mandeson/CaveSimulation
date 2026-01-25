#include "clock.h"
#include "util/time.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

static void *clock_thread(void *arg) {
    Clock *clock = (Clock *)arg;

    struct timeval last_time;
    if (gettimeofday(&last_time, NULL) == -1) {
        perror("clock_thread: gettimeofday");
        return NULL;
    }

    while (!clock->terminate) {
        safe_usleep(MILLISECONDS_IN_SECOND);
        struct timeval time;
        if (gettimeofday(&time, NULL) == -1) {
            perror("clock_thread: gettimeofday");
            return NULL;
        }

        struct timeval diff;
        timersub(&time, &last_time, &diff);
        if (diff.tv_usec > MILLISECONDS_IN_SECOND) {
            clock->shared_memory->time++;
            last_time = time;
        }
    }
    
    return NULL;
}

void clock_init(Clock *clock, SharedMemory *shared_memory) {
    clock->shared_memory = shared_memory;
    clock->terminate = false;
    if (pthread_create(&clock->thread, NULL, clock_thread, clock) != 0)
        fprintf(stderr, "Error: clock_init: pthread_create\n");
}

void clock_destroy(Clock *clock) {
    clock->terminate = true;
    if (pthread_join(clock->thread, NULL) != 0)
        fprintf(stderr, "Error: clock_destroy: pthread_join\n");
}


