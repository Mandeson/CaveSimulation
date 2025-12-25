#pragma once

#define SHARED_MEMORY_ID 59

#define MESSAGE_QUEUE_ID 60

#define SEMAPHORES_ID 61
#define NSEMAPHORES 1

typedef struct {
    int priority_ticket_line_size;
} SharedMemory;

SharedMemory *attach_shared_memory();
void detach_shared_memory(SharedMemory *shared_memory);
int get_message_queue();
int get_semaphores();
