#pragma once

#include "common.h"
#include <pthread.h>

typedef struct {
    pthread_t thread;

    atomic_bool terminate;

    SharedMemory *shared_memory;
} Clock;

void clock_init(Clock *clock, SharedMemory *shared_memory);
void clock_destroy(Clock *clock);
