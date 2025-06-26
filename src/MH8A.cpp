#include <Arduino.h>
#include "MH8A.h"
#include "main.h"
#include "display.h"

#define TIME_MIN_0 800       // us
#define TIME_MAX_0 1200      // us
#define TIME_MIN_1 1800      // us
#define TIME_MAX_1 2200      // us
#define TIME_END_FRAME 15000 // us
#define TIMEOUT 8000000      // us

#define INT_PIN_RECEIVER 4 // GPIO4

// Shared variable between interrupt and main code
volatile long LastTime = 0;
volatile char bufferReception[64] = {0};

String frame = "";

// Section of memory saved during deep sleep of ESP32
RTC_DATA_ATTR uint64_t timestamp = 0;
RTC_DATA_ATTR tHistory history[HISTORY_LENGTH];
RTC_DATA_ATTR int historyIndex = 0;

// Sinusoid 38khz during 1ms + Short pause 1ms is a 0
// sinusoid 38khz during 1ms + long pause 2ms is a 1
// Purpose is to measure the time between the last high level and then to wait the pause to get the next high level
// 0 is then 1ms sinusoid + 1 ms pause
// and 1 is 1ms sinusoid + 2ms pause
void IRAM_ATTR ProcessIntPin()
{
    long Time = micros();
    long Delta = Time - LastTime;

    LastTime = Time;

    if ((Delta > TIME_MIN_0) && (Delta < TIME_MAX_0))
        bufferReception[strlen((char *)bufferReception)] = '0';
    if ((Delta > TIME_MIN_1) && (Delta < TIME_MAX_1))
        bufferReception[strlen((char *)bufferReception)] = '1';
}

// For tank ID, the protocol is using a dedicated coding for each number
// This function will replace the 4 bits of each digit by its integer value
inline String GetChar(String s)
{
    if (s == "1010")
        return "0";
    else if (s == "1011")
        return "1";
    else if (s == "1100")
        return "2";
    else if (s == "0011")
        return "3";
    else if (s == "1101")
        return "4";
    else if (s == "0101")
        return "5";
    else if (s == "0110")
        return "6";
    else if (s == "0111")
        return "7";
    else if (s == "1110")
        return "8";
    else if (s == "1001")
        return "9";
    else
        return "!";
}

// This function will decode the frame that has been received
void Decode(String frameToBeDecoded, int time)
{
    // Display the raw frame
    // Serial.println(frameToBeDecoded);

    // If length is not the good one, exit
    if (frameToBeDecoded.length() != 58)
    {
        Serial.printf("NOK %d\n", frameToBeDecoded.length());
        return;
    }

    // Get the different parts of the frame
    String Preamble = frameToBeDecoded.substring(0, 2);
    String Sync1 = frameToBeDecoded.substring(2, 6);
    String Sync2 = frameToBeDecoded.substring(6, 10);

    // For ID, 6 digits to be replace with the lookup table
    String ID = "";
    for (int i = 0; i < 6; i++)
        ID += GetChar(frameToBeDecoded.substring(10 + i * 4, 10 + 4 + i * 4));

    // Pressure is encoded in PSI - 12bits
    // The value in the frame is half of the real value
    int Pressure = std::stoi(frameToBeDecoded.substring(34, 46).c_str(), nullptr, 2);

    // Battery status can be Good, Low, Critical with a specific coding
    String Batt = frameToBeDecoded.substring(46, 50);
    Batt = (Batt == "0000") ? "Good" : (Batt == "0010") ? "Low"
                                   : (Batt == "0001")   ? "Critical"
                                                        : "Unknown";

    // Checksum is calculated as following :
    // Remove the 2 first bits (Preamble)
    // Add the integer value of each 4 bits (12 nibbles)
    int ChecksumCalc = 0;
    for (int i = 0; i < 12; i++)
        ChecksumCalc += std::stoi(frameToBeDecoded.substring(2 + i * 4, 2 + 4 + i * 4).c_str(), nullptr, 2);

    // Checksum is located in the 8 last bits of the frame
    int ChesumMsg = std::stoi(frameToBeDecoded.substring(50, 58).c_str(), nullptr, 2);

    // Print all data
    Serial.printf("Time : %.1f, ", float(time) / 1000000);
    Serial.printf("ID : %s, ", ID.c_str());
    Serial.printf("Pressure : %d PSI - %.2f bars, ", Pressure * 2, Pressure * 2 / 14.504);
    Serial.printf("Battery : %s, ", Batt.c_str());
    Serial.printf("Checksum : %s\n", (ChecksumCalc == ChesumMsg) ? "OK" : "NOK");
    Serial.flush();

    // Print data on SSD1306 screen
    if (ChecksumCalc == ChesumMsg)
    {
        displayText(main, 2,
                    "ID: %s\nP : %.2f\nB : %s",
                    ID.c_str(), Pressure * 2 / 14.504, Batt.c_str());

        time_t now = timestamp + micros() / 1000000;
        struct tm t;
        localtime_r(&now, &t);

        unsigned char index = historyIndex % HISTORY_LENGTH;

        history[index].num = historyIndex;
        strcpy(history[index].ID, ID.c_str());
        history[index].Pressure = Pressure;
        strcpy(history[index].Battery, Batt.c_str());

        history[index].time = t;

        historyIndex++;

        // Update the live indicator & time
        LiveIndicatorAndTime();
        razTimerGoToSleep();
    }
}

void EmptyBuffer()
{
    int l = strlen((char *)bufferReception);
    for (int i = 0; i < l; i++)
        bufferReception[i] = 0;
}

void loopMH8A()
{
    long TimeFrame = micros();
    static bool NoComm = false;

    // No high value during long time -> end of frame
    if ((TimeFrame - LastTime > TIME_END_FRAME) && (strlen((char *)bufferReception) > 0))
    {
        // Comm active as we received a frame
        NoComm = false;

        // Decode the frame and display it on the console
        frame = (char *)bufferReception;
        Decode(frame, TimeFrame);

        // Init of variables
        frame = "";
        EmptyBuffer();
    }

    // No high value during a longer time -> no more communication
    if ((TimeFrame - LastTime > TIMEOUT) && (NoComm == false))
    {
        Serial.printf("No more communication\n");

        displayText(bottomLeftMid, 1, "No comm");

        // Init of variables
        frame = "";
        EmptyBuffer();

        // No comm
        NoComm = true;
    }
}

void initMH8A()
{
    // Reading will be done trough interrupt thanks to AOP on the board
    pinMode(INT_PIN_RECEIVER, INPUT);
    attachInterrupt(digitalPinToInterrupt(INT_PIN_RECEIVER), ProcessIntPin, RISING);
}