#include "clock.h"
#include <stdbool.h>
#include <sys/time.h>
#include <unistd.h>

static void *clock_thread(void *arg) {
    Clock *clock = (Clock *)arg;

    struct timeval last_time;
    gettimeofday(&last_time, NULL);

    while (!clock->terminate) {
        //usleep(1000);
        struct timeval time;
        gettimeofday(&time, NULL);

        struct timeval diff;
        timersub(&time, &last_time, &diff);
        if (diff.tv_usec > 1000) {
            clock->shared_memory->time++;
            last_time = time;
        }
    }
    
    return NULL;
}

void clock_init(Clock *clock, SharedMemory *shared_memory) {
    clock->shared_memory = shared_memory;
    clock->terminate = false;
    pthread_create(&clock->thread, NULL, clock_thread, clock);
}

void clock_destroy(Clock *clock) {
    clock->terminate = true;
    pthread_join(clock->thread, NULL);
}


