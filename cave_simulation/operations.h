#include "common.h"

int create_shared_memory(SharedMemory **shared_memory_ptr);
int destroy_shared_memory(SharedMemory **shared_memory_ptr, int shared_memory_id);

int create_message_queue(int id);
int destroy_message_queue(int message_queue);

int create_semaphores();
int destroy_semaphores(int semaphores);
