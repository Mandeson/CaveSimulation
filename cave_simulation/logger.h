#pragma once

#include <stdbool.h>
#include <pthread.h>

#define TERMINATE_TEXT "terminate"

#define TIMESTAMP_LENGTH 2 + 8

#define MAX_PROG_LENGTH 16

typedef struct {
    bool log_to_stdout;

    int message_queue;

    int file_main;
    int file_ticket_clerk;
    int file_guide;
    int file_visitor;

    pthread_t logger_thread;
} Logger;

int logger_init(Logger *logger, bool log_to_stdout);
void logger_destroy(Logger *logger);
void* logger_thread_function(void *arg);
