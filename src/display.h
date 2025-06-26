#pragma once

#include "main.h"

typedef enum
{
    main = 0,
    bottomLeftHigh,
    bottomLeftMid,
    bottomRightHigh,
    bottomRightMid,
    bottomMid,
} tZone;

void clearBottom();

void clearBottomLeft();

void clearBottomRight();

void clearMain();

void displayText(tZone zone, int size, const char *format, ...);

void deactivateDisplay();

void initDisplay();
