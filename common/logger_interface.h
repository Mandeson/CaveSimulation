#pragma once

#include "common.h"
typedef struct {
    int logger_message_queue;
    char tag[25];
    const SharedMemory *shared_memory;
} LoggerInterface;

typedef struct {
    long mtype;
    char mtext[256];
} LogMessage;

void logger_interface_new(LoggerInterface *logger, const char *tag, const SharedMemory *shared_memory);
void logger_log(const LoggerInterface *logger, const char *format, ...);
