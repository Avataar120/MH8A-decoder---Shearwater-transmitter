#include <Arduino.h>
#include "esp_timer.h"

#define ADC_PIN 4 // GPIO4 = ADC1_CHANNEL_3 sur l'ESP32-S3

typedef enum
{
  waitForBit,
  receiveBit,
  decodeFrame,
  waitForFrame,
  nocomm
} tReceptionState;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(4);
  analogSetAttenuation(ADC_0db);
  Serial.println("Initialisation terminée. Lecture ADC en cours...");
}

String GetChar(String s)
{
  if (s == "1010")
    return "0";
  if (s == "1011")
    return "1";
  if (s == "1100")
    return "2";
  if (s == "0011")
    return "3";
  if (s == "1101")
    return "4";
  if (s == "0101")
    return "5";
  if (s == "0110")
    return "6";
  if (s == "0111")
    return "7";
  if (s == "1110")
    return "8";
  if (s == "1001")
    return "9";
  return "!";
}

void Decode(String s)
{
  Serial.println(s);
  if (s.length() == 58)
  {

    String Preamble = s.substring(0, 2);
    String Sync1 = s.substring(2, 6);
    String Sync2 = s.substring(6, 10);

    String ID = "";
    for (int i = 0; i < 6; i++)
    {
      ID += GetChar(s.substring(10 + i * 4, 10 + 4 + i * 4));
    }

    int Pressure = std::stoi(s.substring(34, 46).c_str(), nullptr, 2);

    String Batt = s.substring(46, 50);
    if (Batt = "0000")
      Batt = "OK";
    else if (Batt = "0010")
      Batt = "Low";
    else if (Batt = "0001")
      Batt = "Critical";

    int ChecksumCalc = 0;
    for (int i = 0; i < 12; i++)
    {
      int v = std::stoi(s.substring(2 + i * 4, 2 + 4 + i * 4).c_str(), nullptr, 2);
      ChecksumCalc += v;
    }

    int ChesumMsg = std::stoi(s.substring(50, 58).c_str(), nullptr, 2);

    Serial.printf("Preamble : %s\n", Preamble.c_str());
    Serial.printf("Sync1 : %s\n", Sync1.c_str());
    Serial.printf("Sync2 : %s\n", Sync2.c_str());
    Serial.printf("ID : %s\n", ID.c_str());
    Serial.printf("Pressure : %d PSI - %.2f bars\n", Pressure * 2, Pressure * 2 / 14.504);
    Serial.printf("Battery : %s\n", Batt);
    Serial.printf("Checksum calc : %d - Checksum message : %d - ", ChecksumCalc, ChesumMsg);
    if (ChecksumCalc == ChesumMsg)
      Serial.printf("OK\n\n");
    else
      Serial.printf("NOK\n\n");
  }
}

void loop()
{

  static tReceptionState state = waitForBit;
  static uint32_t start = 0;
  static String s;

  int adcValue = analogReadMilliVolts(ADC_PIN);
  bool Peak = (adcValue > 80) ? true : false;

  uint32_t delta = 0;
  static uint32_t SumPeaks = 0;
  static uint32_t NbPeaks = 0;

  uint32_t time = esp_timer_get_time();

  switch (state)
  {
  case waitForBit:
    start = time;

    if (Peak)
      state = receiveBit;

    break;

  case receiveBit:
    delta = time - start;
    if (Peak)
    {
      SumPeaks += adcValue;
      NbPeaks++;

      if ((delta > 1000) && (delta <= 2300))
      {
        s += "0";
        state = waitForBit;
      }
      else if ((delta > 2300) && (delta <= 5000))
      {
        s += "1";
        state = waitForBit;
      }
    }
    if (delta > 15000)
      state = decodeFrame;

    break;

  case decodeFrame:
    Serial.printf("Average Peaks : %f\n", (float)SumPeaks / NbPeaks);
    Decode(s);
    s = "";
    SumPeaks = 0;
    NbPeaks = 0;

    state = waitForFrame;
    break;

  case waitForFrame:
    if (Peak)
    {
      // Serial.printf("Time : %d\n", time - start);
      start = time;
      state = receiveBit;
    }
    else if (time - start > 10000000)
    {
      Serial.printf("No comm\n");
      state = waitForBit;
    }
    break;
  }
}
