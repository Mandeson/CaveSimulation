#pragma once

#include "common.h"
typedef struct {
    int hours;
    int minutes;
    int seconds;
} Time;

Time time_from_seconds(int time, const SharedMemory *shared_memory);
void time_to_string(char string[9], Time time);
