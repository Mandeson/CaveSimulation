#pragma once

#include <stdbool.h>
#include <pthread.h>

typedef struct {
    bool log_to_stdout;

    int message_queue;
    int file;

    pthread_t logger_thread;
} Logger;

void logger_init(Logger *logger, bool log_to_stdout);
void logger_destroy(Logger *logger);
void* logger_thread_function(void *arg);
