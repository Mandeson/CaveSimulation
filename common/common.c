#include "common.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>

SharedMemory *attach_shared_memory() {
    key_t shared_memory_key = ftok(".",SHARED_MEMORY_ID);
    if (shared_memory_key == -1) {
        perror("attach_shared_memory: ftok");
        return NULL;
    }

    int shared_memory = shmget(shared_memory_key, sizeof(SharedMemory), IPC_CREAT | 0600);
    if (shared_memory == -1) {
        perror("attach_shared_memory: shmget");
        return NULL;
    }

    void *shared_memory_addr = shmat(shared_memory, NULL, 0);
    if (shared_memory_addr == (void *)-1) {
        perror("attach_shared_memory: shmat");
        return NULL;
    }

    return (SharedMemory *)shared_memory_addr;
}

int detach_shared_memory(SharedMemory *shared_memory) {
    int res = shmdt((void *)shared_memory);
    if (res == -1)
        perror("detach_shared_memory: shmdt");
    return res;
}

int get_message_queue(int id) {
    key_t message_queue_key = ftok(".", id);
    if (message_queue_key == -1) {
        perror("get_message_queue: ftok");
        return -1;
    }

    int message_queue = msgget(message_queue_key, IPC_CREAT | 0600);
    if (message_queue == -1)
        perror("get_message_queue: msgget");

    return message_queue;
}

int get_semaphores() {
    key_t semaphore_key = ftok(".",SEMAPHORES_ID);
    if (semaphore_key == -1) {
        perror("get_semaphores: ftok");
        return -1;
    }

    int semaphores = semget(semaphore_key, NSEMAPHORES, IPC_CREAT | 0600);
    if (semaphores == -1)
        perror("get_semaphores: semget");

    return semaphores;
}

int take_semaphore(int semaphores, int number) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = -1;
    op.sem_flg = SEM_UNDO;

    if (semop(semaphores, &op, 1) == -1 && errno != EINTR) {
        perror("take_semaphore: semop");
        return -1;
    }

    // if (number == SHARED_MEMORY_SEMAPHORE) {
    //     take_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    //     printf("PID: %d takes shm semaphore\n", getpid());
    //     fflush(stdout);
    //     give_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    // }

    return 0;
}

int take_semaphore_n(int semaphores, int number, int n) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = -n;
    op.sem_flg = 0;

    if (semop(semaphores, &op, 1) == -1) {
        perror("take_semaphore_n: semop");
        return -1;
    }

    return 0;
}

int give_semaphore(int semaphores, int number) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = 1;
    op.sem_flg = SEM_UNDO;

    // if (number == SHARED_MEMORY_SEMAPHORE) {
    //     take_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    //     printf("PID: %d gives shm semaphore\n", getpid());
    //     fflush(stdout);
    //     give_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    // }

    if (semop(semaphores, &op, 1) == -1 && errno != EINTR) {
        perror("give_semaphore: semop");
        return -1;
    }

    return 0;
}

int give_semaphore_n(int semaphores, int number, int n) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = n;
    op.sem_flg = 0;

    if (semop(semaphores, &op, 1) == -1) {
        perror("give_semaphore_n: semop");
        return -1;
    }

    return 0;
}

int set_semaphore(int semaphores, int number, int n) {
    if (semctl(semaphores, number, SETVAL, n) == -1) {
        perror("give_semaphore_n: semop");
        return -1;
    }

    return 0;
}

void logger_interface_new(LoggerInterface *logger, const char *tag) {
    strncpy(logger->tag, tag, sizeof(logger->tag) - 1);

    logger->logger_message_queue = get_message_queue(LOGGER_MESSAGE_QUEUE_ID);
}

void logger_log(const LoggerInterface *logger, const char *format, ...) {
    va_list arg;
    LogMessage message;
    size_t size = sizeof(message.mtext) - 1 - strlen(logger->tag) - 2;
    char *string = malloc(size);
    if (string == NULL) {
        perror("logger_log: malloc");
        return;
    }

    va_start(arg, format);
    vsnprintf(string, size - 1, format, arg);
    va_end(arg);

    message.mtype = 1;
    message.mtext[0] = '\0';
    strcat(message.mtext, logger->tag);
    strcat(message.mtext, ": ");
    strcat(message.mtext, string);

    if (msgsnd(logger->logger_message_queue, &message, sizeof(message.mtext), 0) == -1) {
        perror("logger_log: msgsnd");
    }

    free(string);
}
