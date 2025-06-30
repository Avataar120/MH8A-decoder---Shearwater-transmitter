#include <Arduino.h>

#include "main.h"
#include "web.h"
#include "display.h"
#include "MH8A.h"

#define ADC_PIN_BATTERY 10 // GPIO10

#define PIN_MOSFET_33V 7 // GPIO 7
#define PIN_WAKE_UP 0    // GPIO 0

#define TIME_TO_SLEEP 60000  // ms
#define SLEEP_DURATION_SEC 5 // s

#define NB_BATTERY_FILTER 5 // # of values for filtering

bool FirstTime = 1;

unsigned long startMillis;

// Read battery voltage and filter it on NB_BATTERY_FILTER values
void ComputeBatteryVoltage()
{
  static int startMicros = 0;
  float Result = -1.0;

  if (micros() - startMicros < 500000)
  {
    return;
  }

  static int Array[NB_BATTERY_FILTER] = {0}; // Buffer to store the values
  static int Next = NB_BATTERY_FILTER - 1;   // Position of the next value to write
  static int Nb = 0;                         // Nb of values already read
  static int Sum = 0;                        // Sum of values
  int LastValue = 0;                         // Last value (to be removed from the sum)

  int adcValueReceiver = analogReadMilliVolts(ADC_PIN_BATTERY);

  // Put index on the next slot in the buffer - reset to 0 if end of array
  Next++;
  if (Next >= NB_BATTERY_FILTER)
    Next = 0;

  // get the value that is currently in this slot as it will be overwritten
  LastValue = Array[Next];

  // Put the new value & add to the sum
  Array[Next] = adcValueReceiver;
  Sum += adcValueReceiver;

  // If array still not full, just inc the number of values
  if (Nb < NB_BATTERY_FILTER)
    Nb++;
  else
  // else, we remove from the sum the saved value and return the filtered voltage
  {
    Sum -= LastValue;
    Result = Sum * 3.0 / NB_BATTERY_FILTER / 1000;
  }

  // If filetring finished -> display the batterie level
  if (Result != -1.0)
  {
    if (FirstTime)
    {
      clearBottom();
      FirstTime = 0;
    }
    else
      clearBottomRight();

    displayText(bottomRightMid, 1, "%.2fV", Result);
  }

  startMicros = micros();
}

void activateBoardPower()
{
  gpio_hold_dis((gpio_num_t)PIN_MOSFET_33V);
  pinMode(PIN_MOSFET_33V, OUTPUT);
  digitalWrite(PIN_MOSFET_33V, LOW); // Mosfet activated when grid at zero
}

void deactivateBoardPower()
{
  digitalWrite(PIN_MOSFET_33V, HIGH); // Mosfet deactivated when grid at 3.3v
  pinMode(PIN_MOSFET_33V, INPUT);
  gpio_hold_en((gpio_num_t)PIN_MOSFET_33V);

  gpio_deep_sleep_hold_en();
}

void setup()
{
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  if (cause == ESP_SLEEP_WAKEUP_TIMER)
  {
    // if wakeup by timer, add sleep duration to global timestamp
    timestamp += SLEEP_DURATION_SEC;

    // Immediately go back to deep sleep
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKE_UP, LOW);
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SEC * 1000000ULL);
    esp_deep_sleep_start();
  }
  else if (cause == ESP_SLEEP_WAKEUP_EXT0)
  {
    // Add half of the sleep duration to the wakeup timer (as it is not possible to measure the time asleep)
    timestamp += SLEEP_DURATION_SEC / 2;
  }
  else
  {
    // First start
    timestamp = 1735689600; // 01/01/2025 - 00:00:00
  }

  // Serial init
  Serial.begin(115200);
  delay(500);

  // Set attenuation at 11dB to read up to 2V
  analogReadResolution(8);
  analogSetAttenuation(ADC_11db);

  activateBoardPower();

  delay(200);

  initDisplay();
  initMH8A();

  // Used to go to sleep
  startMillis = millis();
}

void goToSleep()
{
  deactivateDisplay();
  deactivateBoardPower();

  // Wake up with GPIO
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_WAKE_UP, LOW);

  // Wake up with Timer
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SEC * 1000000ULL);

  // Update timestamp
  timestamp += micros() / 1000000;

  esp_deep_sleep_start();
}

// Display live indicator
void LiveIndicatorAndTime()
{
  const char Buffer[4] = {'|',
                          '/',
                          '-', '\\'};

  static unsigned char index = 0;

  time_t now = timestamp + micros() / 1000000;

  struct tm t;
  localtime_r(&now, &t);

  displayText(bottomLeftHigh, 1, "%02d-%02d-%02d\n%02d:%02d:%02d",
              t.tm_year + 1900 - 2000, t.tm_mon + 1, t.tm_mday,
              t.tm_hour, t.tm_min, t.tm_sec);

  displayText(bottomMid, 1, "%c", Buffer[index]);

  index++;
  if (index >= 4)
    index = 0;
}

void razTimerGoToSleep()
{
  startMillis = millis();
}

void updateTime(time_t epoch)
{
  timestamp = epoch;
}

void loop()
{

  static int TimeToActivateWeb = 0;

  loopWeb();

  // Go to sleep after xx secods
  if (millis() - startMillis < TIME_TO_SLEEP)
  {

    // When button is pressed, raz of wake up timer & update time
    if (digitalRead(PIN_WAKE_UP) == LOW)
    {
      razTimerGoToSleep();
      LiveIndicatorAndTime();

      if (millis() - TimeToActivateWeb > 2000)
      {
        initWeb();

        displayText(bottomLeftMid, 1, "Wifi Activated");

        sleep(2);
      }
    }
    else
    {
      TimeToActivateWeb = millis();
    }

    // Compute battery level of the receiver
    ComputeBatteryVoltage();

    // Read MH8A frames
    loopMH8A();
  }
  else
  {
    Serial.printf("Going to deep sleep\n");

    clearBottom();
    displayText(bottomLeftHigh, 2, "Sleep ...");

    sleep(5);

    goToSleep();
  }
}
