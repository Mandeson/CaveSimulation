#include "time.h"
#include <stdio.h>

Time time_from_seconds(int time, const SharedMemory *shared_memory) {
    time += shared_memory->parameters.Tp * 3600;
    Time result;
    result.hours = time / 3600;
    result.minutes = (time % 3600) / 60;
    result.seconds = time % 60;
    return result;
}

void time_to_string(char string[9], Time time) {
    snprintf(string, 9, "%02d:%02d:%02d", time.hours, time.minutes, time.seconds);
}
