#pragma once

#define MILLISECONDS_IN_SECOND 1000
#define SECONDS_IN_MINUTE 60

#define TRIPS_DOABLE_BEFORE_CLOSING 5 / 2

#include "common.h"
typedef struct {
    int hours;
    int minutes;
    int seconds;
} Time;

Time time_from_seconds(int time, const SimulationParameters *parameters);
void time_to_string(char string[9], Time time);
int calculate_closing_time(const SimulationParameters *parameters);
