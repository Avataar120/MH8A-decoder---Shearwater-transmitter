#pragma once

typedef struct
{
    int num;
    tm time;
    char ID[7];
    int Pressure;
    char Battery[9];
} tHistory;

extern void updateTime(time_t epoch);

#define HISTORY_LENGTH 100