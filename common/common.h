#pragma once

#include <stdbool.h>
#include <stdint.h>
#define SHARED_MEMORY_ID 59

#define MESSAGE_QUEUE_ID 60

#define SEMAPHORES_ID 61
#define NSEMAPHORES 4
#define OUTPUT_LOG_SEMAPHORE 0
#define SHARED_MEMORY_SEMAPHORE 1
#define TICKET_REGULAR_SEMAPHORE 2
#define TICKET_PRIORITY_SEMAPHORE 3

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

typedef struct {
    char output_file_name[256];
    volatile bool terminating;
    int ticket_clerk_pid;
    volatile int visitors_count;
    volatile int visitors_finished;
    volatile int priority_ticket_line_size;
    volatile int regular_ticket_line_size;
} SharedMemory;

typedef struct {
    long mtype;
    char mtext[16];
} Message;

typedef struct {
    int age;
    uint8_t children_count;
    uint8_t children_ages[7];
} VisitorInfo;

typedef struct {
    int pid;
    VisitorInfo visitor_info;
} VisitorMessage;

typedef struct {
    uint8_t trail_nr;
    int cost;
} TicketMessage;

SharedMemory *attach_shared_memory();
int detach_shared_memory(SharedMemory *shared_memory);
int get_message_queue();
int get_semaphores();
int take_semaphore(int semaphores, int number);
int give_semaphore(int semaphores, int number);
void output_log(int semaphores, const SharedMemory *shared_memory, const char *format, ...);
