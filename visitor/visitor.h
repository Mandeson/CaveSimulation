#pragma once

#include <stdbool.h>

typedef struct {
    int age;
    bool has_child;

    int message_queue;
    int semaphores;
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
