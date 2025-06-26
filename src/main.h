#pragma once

typedef struct
{
    int num;
    tm time;
    char ID[7];
    int Pressure;
    char Battery[9];
} tHistory;

void LiveIndicatorAndTime();

void razTimerGoToSleep();

void goToSleep();

void updateTime(time_t epoch);

#define HISTORY_LENGTH 100