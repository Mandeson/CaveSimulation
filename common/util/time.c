#include "time.h"
#include "common.h"
#include <stdio.h>

Time time_from_seconds(int time, const SimulationParameters *parameters) {
    time += parameters->Tp * 3600;
    Time result;
    result.hours = time / 3600;
    result.minutes = (time % 3600) / 60;
    result.seconds = time % 60;
    return result;
}

void time_to_string(char string[9], Time time) {
    snprintf(string, 9, "%02d:%02d:%02d", time.hours, time.minutes, time.seconds);
}

int calculate_closing_time(const SimulationParameters *parameters) {
    int max_trip_time = MAX(parameters->T[0], parameters->T[1]);
    return (parameters->Tk - parameters->Tp) * 3600 - max_trip_time * 60 * 2;
}

