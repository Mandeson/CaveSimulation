#pragma once

#include "common.h"
#include <stdbool.h>

typedef struct {
    SharedMemory *shared_memory;
    int message_queue;
    int semaphores;
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
