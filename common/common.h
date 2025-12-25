#pragma once

#define SHARED_MEMORY_ID 59

#define MESSAGE_QUEUE_ID 60

#define SEMAPHORES_ID 61
#define NSEMAPHORES 1
#define OUTPUT_LOG_SEMAPHORE 0

typedef struct {
    char output_file_name[256];
    int priority_ticket_line_size;
} SharedMemory;

SharedMemory *attach_shared_memory();
void detach_shared_memory(SharedMemory *shared_memory);
int get_message_queue();
int get_semaphores();
int take_semaphore(int semaphores, int number);
int give_semaphore(int semaphores, int number);
void output_log(int semaphores, const SharedMemory *shared_memory, const char *format, ...);
