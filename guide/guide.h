#pragma once

#include <stdbool.h>
#include "common.h"
#include "util/array.h"

typedef struct {
    int number;
    int trail_visitors_count;
    Array trail_visitors;
    volatile bool tour_cancelled;
    bool terminate;

    SharedMemory *shared_memory;
    int message_queue;
    int semaphores;
    LoggerInterface logger;
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
