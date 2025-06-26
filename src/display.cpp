#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "display.h"

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define I2C_SDA 35
#define I2C_SCL 36

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void clearBottom()
{
    display.fillRect(0, 48, 128, 16, BLACK);
    display.drawLine(0, 47, 128, 47, WHITE);
}

void clearBottomLeft()
{
    display.fillRect(0, 48, 94, 16, BLACK);
    display.drawLine(0, 47, 128, 47, WHITE);
}

void clearBottomRight()
{
    display.fillRect(95, 48, 128 - 95, 16, BLACK);
    display.drawLine(0, 47, 128, 47, WHITE);
}

void clearMain()
{
    display.fillRect(0, 0, 128, 47, BLACK);
    display.drawLine(0, 47, 128, 47, WHITE);
}

void displayText(tZone zone, int size, const char *format, ...)
{
    int x = 0, y = 0;

    char buffer[256]; // Buffer pour stocker le texte formaté

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    switch (zone)
    {
    case main:
        x = y = 0;
        clearMain();
        break;

    case bottomLeftHigh:
        x = 0;
        y = 48;
        clearBottomLeft();
        break;

    case bottomLeftMid:
        x = 0;
        y = 52;
        clearBottomLeft();
        break;

    case bottomRightHigh:
        x = 95;
        y = 48;
        clearBottomRight();
        break;

    case bottomRightMid:
        x = 95;
        y = 52;
        clearBottomRight();
        break;

    case bottomMid:
        x = 64;
        y = 52;
        // clearBottomLeft();
        break;
    }

    display.setTextSize(size);
    display.setCursor(x, y);
    display.printf(buffer);
    display.display();
}

void deactivateDisplay()
{
    pinMode(I2C_SDA, INPUT);
    pinMode(I2C_SCL, INPUT);
    Wire.end();
}

void initDisplay()
{
    // Init of I2C for SSD1306
    pinMode(I2C_SDA, INPUT_PULLUP);
    pinMode(I2C_SCL, INPUT_PULLUP);
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("Display init failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }

    delay(500);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setRotation(2);
    display.setTextColor(SSD1306_WHITE);

    displayText(bottomLeftHigh, 2, "Booting ...");

    display.display();
}