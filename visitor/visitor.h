#pragma once

#include "common.h"
#include "logger_interface.h"
#include <stdbool.h>

#define VISITOR_MIN_AGE 1
#define VISITOR_MAX_AGE 80
#define VISITOR_TRAIL_1_MAX_AGE 75

typedef struct {
    VisitorInfo visitor_info;

    SharedMemory *shared_memory;
    int message_queue;
    int semaphores;
    LoggerInterface logger;
    volatile atomic_bool logger_initialized;
} Visitor;

typedef enum {
    VISITOR_SUCCESS = 0,

    VISITOR_INIT_FAIL,
    VISITOR_DESTROY_FAIL,
    VISITOR_RUN_FAIL
} VisitorRes;

VisitorRes visitor_init(Visitor *visitor);
VisitorRes visitor_destroy(Visitor *visitor);
VisitorRes visitor_run(Visitor *visitor);
