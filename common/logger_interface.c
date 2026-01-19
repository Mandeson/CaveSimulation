#include "logger_interface.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include "common.h"
#include "util/time.h"

void logger_interface_new(LoggerInterface *logger, const char *tag, const SharedMemory *shared_memory) {
    logger->shared_memory = shared_memory;
    strncpy(logger->tag, tag, sizeof(logger->tag) - 1);
    logger->logger_message_queue = get_message_queue(LOGGER_MESSAGE_QUEUE_ID);
}

void logger_log(const LoggerInterface *logger, const char *format, ...) {
    va_list arg;
    LogMessage message;
    char string[226];

    va_start(arg, format);
    vsnprintf(string, sizeof(string), format, arg);
    va_end(arg);

    int time = logger->shared_memory ? logger->shared_memory->time : -1;
    char time_string[9];
    time_to_string(time_string, time_from_seconds(time, logger->shared_memory));

    char str[sizeof(message.mtext) + 1];
    snprintf(str, sizeof(str), "[%s] %s: %s", time_string,
            logger->tag, string);
    size_t len = strlen(str);

    message.mtype = 1;
    memcpy(message.mtext, str, len);

    while (msgsnd(logger->logger_message_queue, &message, len, 0) == -1) {
        if (errno != EINTR) {
            perror("logger_log: msgsnd");
            break;
        }
    }
}
