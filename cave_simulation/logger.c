#include "logger.h"

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "operations.h"

int logger_init(Logger *logger, bool log_to_stdout) {
    logger->log_to_stdout = log_to_stdout;

    logger->message_queue = create_message_queue(LOGGER_MESSAGE_QUEUE_ID);
    if (logger->message_queue == -1)
        return -1;

    // Get time to set the output directory name
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char time_date[128];
    snprintf(time_date, 128,
            "%d-%02d-%02d_%02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    const char *log_directory = "./log";
    char tmp[256];

    if (mkdir(log_directory, 0700) == -1 && errno != EEXIST) {
        perror("logger_init: mkdir 1");
        return -1;
    }

    snprintf(tmp, 256, "%s/%s", log_directory, time_date);
    if (mkdir(tmp, 0700) == -1 && errno != EEXIST) {
        perror("logger_init: mkdir 2");
        return -1;
    }

    snprintf(tmp, 256, "%s/%s/main.txt", log_directory, time_date);
    printf("Creating main log file: %s\n", tmp);
    fflush(stdout);
    int fd = open(tmp, O_CREAT | O_WRONLY, 0600);
    if (fd == -1) {
        perror("logger_init: open");
        return -1;
    }
    logger->file = fd;

    pthread_create(&logger->logger_thread, NULL, logger_thread_function, logger);

    return 0;
}

void logger_destroy(Logger *logger) {
    pthread_cancel(logger->logger_thread);
    pthread_join(logger->logger_thread, NULL);

    close(logger->file);

    destroy_message_queue(logger->message_queue);
}

void* logger_thread_function(void *arg) {
    Logger *logger = (Logger *)arg;

    while (true) {
        // Receive log message from message queue
        LogMessage log_message = {0};
        int res = msgrcv(logger->message_queue, &log_message, sizeof(log_message.mtext), 1, 0);
        if (res == -1) {
            if (errno == EINTR)
                continue;
            perror("logger_thread_function: msgrcv");
            break;
        }
        if (res != sizeof(log_message.mtext)) {
            fprintf(stderr, "logger_thread_function: wrong number of bytes received"
                    "from message queue\n");
            continue;
        }

        if (logger->log_to_stdout) {
            // Print log message to stdout
            puts(log_message.mtext);
            fflush(stdout);
        }

        char message[sizeof(log_message.mtext)];
        strcpy(message, log_message.mtext);

        int len = strlen(message);

        // Add new line character
        message[len] = '\n';

        if (write(logger->file, message, len + 1) == -1) {
            perror("logger_thread_function: write");
            break;
        }
    }

    return NULL;
}
