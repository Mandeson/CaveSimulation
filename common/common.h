#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHARED_MEMORY_ID 59

#define MESSAGE_QUEUE_ID 60
#define LOGGER_MESSAGE_QUEUE_ID 61

#define SEMAPHORES_ID 62
#define NSEMAPHORES 6
#define SHARED_MEMORY_SEMAPHORE 0
#define TICKET_REGULAR_SEMAPHORE 1
#define TICKET_PRIORITY_SEMAPHORE 2
#define WAITING_BY_GUIDE1_SEMAPHORE 3
#define WAITING_BY_GUIDE2_SEMAPHORE 4
#define CATWALK_SEMAPHORE 5

// Delay between ticket sales
#define TICKET_CLERK_DELAY 10 //In simulation seconds

#define GUIDE_COUNT 2
#define VISITORS_WAITING_SIZE 1024

#define MESSAGE_QUEUE_SEND_SUCCESS 0
#define MESSAGE_QUEUE_SEND_FAIL -1

#define MESSAGE_QUEUE_RECEIVE_NO_MESSAGE 1
#define MESSAGE_QUEUE_RECEIVE_SUCCESS 0
#define MESSAGE_QUEUE_RECEIVE_FAIL -1

#define MESSAGE_QUEUE_MARGIN 8

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

typedef struct {
    int Tp; // Start time (in hours)
    int Tk; // End time (in hours)
    int N[2]; // Number of visitors for each trail
    int T[2]; // Tour time for each trail (in minutes)
    int K; // Capacity of the catwalks
} SimulationParameters;

typedef struct {
    int pid;
    int children_count;
} VisitorWaiting;

typedef struct {
    // Set on initialization
    
    SimulationParameters parameters;

    int ticket_clerk_pid;
    int guide1_pid;
    int guide2_pid;

    int catwalk_pipe[2][2];


    // Changable variables

    volatile atomic_int processes_starting;

    volatile atomic_int time;

    volatile atomic_int catwalk_visitors[2];

    volatile atomic_bool terminating;
    volatile atomic_bool interrupted;


    // Protected by semaphores

    volatile VisitorWaiting visitors_waiting[GUIDE_COUNT][VISITORS_WAITING_SIZE];
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
    uint8_t children_ages[2];
    bool second_tour;
    int trail_nr;
} VisitorInfo;

typedef struct {
    int pid;
    VisitorInfo visitor_info;
} VisitorMessage;

typedef struct {
    uint8_t trail_nr;
    int cost;
} TicketMessage;

typedef struct {
    // Indicates whether the visitor is allowed to enter the cave or needs to go back
    bool entering_cave;
} VisitorEnterMessage;

typedef struct {
    int pid;
    int children_count;
    int trail_nr;
} VisitorOnCatwalk;

SharedMemory *attach_shared_memory();
int detach_shared_memory(SharedMemory **shared_memory);
int get_message_queue(int id);
int get_semaphores();
int take_semaphore(int semaphores, int number);
int give_semaphore(int semaphores, int number);
int message_queue_send(int message_queue, long type, const void *data, size_t size, const char *caller);
int message_queue_send_check_ovf(int message_queue, long type, const void *data, size_t size);
int message_queue_receive(int message_queue, long type, Message *message, const char *caller, bool block);
void close_catwalk_pipe_input(const SharedMemory *shared_memory);
void close_catwalk_pipe_output(const SharedMemory *shared_memory);
void safe_usleep(long microseconds);
