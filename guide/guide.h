#pragma once

#include <stdbool.h>
#include "common.h"
#include "logger_interface.h"
#include "util/array.h"

typedef struct {
    int number;
    int waiting_by_guide_semaphore;
    
    int trail_visitors_count;

    Array trail_visitors;
    volatile bool trail_visitors_initialized;

    volatile bool tour_cancelled;
    bool terminate;

    SharedMemory *shared_memory;
    int message_queue;
    int semaphores;

    LoggerInterface logger;
    volatile bool logger_initialized;
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
