#include "common.h"
#include <stdarg.h>
#include <stdio.h>
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

    int shared_memory = shmget(shared_memory_key, sizeof(SharedMemory), IPC_CREAT | 0666);
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

int get_message_queue() {
    key_t message_queue_key = ftok(".",MESSAGE_QUEUE_ID);
    if (message_queue_key == -1) {
        perror("get_message_queue: ftok");
        return -1;
    }

    int message_queue = msgget(message_queue_key, IPC_CREAT | 0666);
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

    int semaphores = semget(semaphore_key, NSEMAPHORES, IPC_CREAT | 0666);
    if (semaphores == -1)
        perror("get_semaphores: semget");

    return semaphores;
}

int take_semaphore(int semaphores, int number) {
    struct sembuf op;
    op.sem_num = number;
    op.sem_op = -1;
    op.sem_flg = 0;//SEM_UNDO;

    if (semop(semaphores, &op, 1) == -1) {
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
    op.sem_flg = 0;//SEM_UNDO;

    // if (number == SHARED_MEMORY_SEMAPHORE) {
    //     take_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    //     printf("PID: %d gives shm semaphore\n", getpid());
    //     fflush(stdout);
    //     give_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
    // }

    if (semop(semaphores, &op, 1) == -1) {
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

void output_log(int semaphores, const SharedMemory *shared_memory, const char *format, ...) {
    va_list arg;
    char string[256];

    va_start(arg, format);
    int cnt = vsnprintf(string, 256 - 1, format, arg);
    va_end(arg);

    string[cnt] = '\n';
    if (cnt + 1 >= 256) // Prevent buffer overflow
        cnt = 254;
    string[cnt + 1] = '\0';

    take_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);

    int fd = open(shared_memory->output_file_name, O_WRONLY | O_APPEND, 0600);
    if (fd == -1) {
        perror("output_log: open");
        give_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
        return;
    }
    if (write(fd, string, cnt + 1) == -1)
        perror("output_log: write");
    if (close(fd) == -1)
        perror("output_log: close");

    give_semaphore(semaphores, OUTPUT_LOG_SEMAPHORE);
}
