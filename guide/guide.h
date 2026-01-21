#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include "common.h"
#include "logger_interface.h"
#include "util/array.h"

typedef struct {
    int number;
    int waiting_by_guide_semaphore;
    
    int trail_visitors_count;

    Array trail_visitors;
    volatile atomic_bool trail_visitors_initialized;

    volatile atomic_bool tour_cancelled;
    bool terminate;

    SharedMemory *shared_memory;
    int message_queue;
    int semaphores;

    LoggerInterface logger;
    volatile atomic_bool logger_initialized;
} Guide;

typedef enum {
    GUIDE_SUCCESS = 0,

    GUIDE_INIT_FAIL,
    GUIDE_DESTROY_FAIL,
    GUIDE_RUN_FAIL
} GuideRes;

GuideRes guide_init(Guide *guide);
GuideRes guide_destroy(Guide *guide);
GuideRes guide_run(Guide *guide);
void guide_signal(Guide *guide);
