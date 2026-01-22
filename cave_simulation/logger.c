#include "logger.h"

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>

#include "logger_interface.h"
#include "operations.h"

int create_log_file(int *fd, const char *filename) {
    *fd = open(filename, O_CREAT | O_WRONLY, 0600);
    if (*fd == -1) {
        perror("logger_init: open");
        return -1;
    }

    return 0;
}

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

    // Create the log directory
    if (mkdir(log_directory, 0700) == -1 && errno != EEXIST) {
        perror("logger_init: mkdir 1");
        return -1;
    }

    // Create the program output directory
    snprintf(tmp, 256, "%s/%s", log_directory, time_date);
    if (mkdir(tmp, 0700) == -1) {
        perror("logger_init: mkdir 2");
        return -1;
    }

    snprintf(tmp, 256, "%s/%s/main.txt", log_directory, time_date);

    printf("Creating main log file: %s\n", tmp);
    fflush(stdout);
    if (create_log_file(&logger->file_main, tmp) == -1)
        return -1;

    snprintf(tmp, 256, "%s/%s/TicketClerk.txt", log_directory, time_date);
    if (create_log_file(&logger->file_ticket_clerk, tmp) == -1)
        return -1;

    snprintf(tmp, 256, "%s/%s/Guide.txt", log_directory, time_date);
    if (create_log_file(&logger->file_guide, tmp) == -1)
        return -1;

    snprintf(tmp, 256, "%s/%s/Visitor.txt", log_directory, time_date);
    if (create_log_file(&logger->file_visitor, tmp) == -1)
        return -1;

    pthread_create(&logger->logger_thread, NULL, logger_thread_function, logger);

    return 0;
}

void logger_destroy(Logger *logger) {
    fflush(stdout);

    LogMessage message = {0};
    message.mtype = 1;
    strcpy(message.mtext, TERMINATE_TEXT);

    while (msgsnd(logger->message_queue, &message, sizeof(TERMINATE_TEXT), 0) == -1) {
        if (errno != EINTR) {
            perror("logger_destroy: msgsnd");
            break;
        }
    }
    pthread_join(logger->logger_thread, NULL);

    if (close(logger->file_main) == -1)
        perror("logger_destroy: close main.txt");

    if (close(logger->file_ticket_clerk) == -1)
        perror("logger_destroy: close TicketClerk.txt");

    if (close(logger->file_guide) == -1)
        perror("logger_destroy: close Guide.txt");

    if (close(logger->file_visitor) == -1)
        perror("logger_destroy: close Visitor.txt");

    destroy_message_queue(logger->message_queue);
}

// Returns true if the message was issued by program 'prog'
static bool is_log_source(const char *message, const char *prog) {
    size_t prog_len = strlen(prog);
    if (strlen(message) < TIMESTAMP_LENGTH + 1 + prog_len)
        return false;
    char extracted_prog_name[MAX_PROG_LENGTH] = {0};
    strncpy(extracted_prog_name, message + TIMESTAMP_LENGTH + 1, prog_len);
    return strcmp(extracted_prog_name, prog) == 0;
}

void* logger_thread_function(void *arg) {
    Logger *logger = (Logger *)arg;

    while (true) {
        // Receive log message from message queue
        LogMessage log_message = {0};
        int length = msgrcv(logger->message_queue, &log_message, sizeof(log_message.mtext), 1, 0);
        if (length == -1) {
            if (errno == EINTR)
                continue;
            perror("logger_thread_function: msgrcv");
            break;
        }
        if (length == 0) {
            fprintf(stderr, "logger_thread_function: empty message\n");
            continue;
        }

        char message[sizeof(log_message.mtext) + 1];
        memcpy(message, log_message.mtext, length);
        message[length] = 0;

        if (strcmp(message, TERMINATE_TEXT) == 0)
            break;

        if (logger->log_to_stdout) {
            // Print log message to stdout
            puts(message);
        }

        int additional_fd = -1;
        if (is_log_source(message, "TicketClerk"))
            additional_fd = logger->file_ticket_clerk;
        else if (is_log_source(message, "Guide"))
            additional_fd = logger->file_guide;
        else if (is_log_source(message, "Visitor"))
            additional_fd = logger->file_visitor;

        // Add new line character
        message[length] = '\n';

        if (write(logger->file_main, message, length + 1) == -1) {
            perror("logger_thread_function: write");
            break;
        }

        if (additional_fd != -1) {
            if (write(additional_fd, message, length + 1) == -1) {
                perror("logger_thread_function: write (additional file)");
                break;
            }
        }
    }

    return NULL;
}
