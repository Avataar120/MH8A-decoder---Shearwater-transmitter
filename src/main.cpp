#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#define INT_PIN_RECEIVER 4 // GPIO4
#define ADC_PIN_BATTERY 10 // GPIO10

#define PIN_MOSFET_33V 7 // GPIO 7
#define PIN_WAKE_UP 0    // GPIO 0

#define TIME_MIN_0 800       // us
#define TIME_MAX_0 1200      // us
#define TIME_MIN_1 1800      // us
#define TIME_MAX_1 2200      // us
#define TIME_END_FRAME 15000 // us
#define TIMEOUT 8000000      // us

#define TIME_TO_SLEEP 60000  // ms
#define SLEEP_DURATION_SEC 5 // s

#define NB_BATTERY_FILTER 5 // # of values for filtering

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

bool FirstTime = 1;
String frame = "";

// Shared variable between interrupt and main code
volatile long LastTime = 0;
volatile char bufferReception[64] = {0};

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define I2C_SDA 35
#define I2C_SCL 36

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long startMillis;
RTC_DATA_ATTR uint64_t timestamp = 0;

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
      display.fillRect(0, 48, 128, 16, BLACK);
      FirstTime = 0;
    }
    else
      display.fillRect(95, 48, 128 - 95, 16, BLACK);

    display.setTextSize(1);
    display.setCursor(95, 52);
    display.printf("%.2fV", Result);
    display.display();
    display.setTextSize(2);
  }

  startMicros = micros();
}

void activateDisplay()
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

  Serial.println("Init succesfull");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setRotation(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 48);
  display.printf("Booting ...");

  display.display();
}

void deactivateDisplay()
{
  pinMode(I2C_SDA, INPUT);
  pinMode(I2C_SCL, INPUT);
  Wire.end();
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

void EmptyBuffer()
{
  int l = strlen((char *)bufferReception);
  for (int i = 0; i < l; i++)
    bufferReception[i] = 0;
}

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

  // Set ADC resolution at the minimum to go faster
  // Set attenuation at 0db as signal is very low (100mV)
  analogReadResolution(8);
  analogSetAttenuation(ADC_11db);

  activateBoardPower();
  activateDisplay();

  // Reading will be done trough interrupt thanks to AOP on the board
  pinMode(INT_PIN_RECEIVER, INPUT);
  attachInterrupt(digitalPinToInterrupt(INT_PIN_RECEIVER), ProcessIntPin, RISING);

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

// Display live indicator
void LiveIndicatorAndTime()
{
  const char Buffer[4] = {'|',
                          '/',
                          '-', '\\'};

  static char index = 0;

  display.setCursor(0, 48);
  display.setTextSize(1);

  time_t now = timestamp + micros() / 1000000;

  struct tm t;
  localtime_r(&now, &t);

  display.fillRect(0, 48, 95, 16, BLACK);

  display.drawLine(0, 47, 128, 47, WHITE);
  display.printf("%02d-%02d-%02d\n%02d:%02d:%02d",
                 t.tm_year + 1900 - 2000, t.tm_mon + 1, t.tm_mday,
                 t.tm_hour, t.tm_min, t.tm_sec);

  display.setCursor(64, 52);
  display.printf("%c", Buffer[index]);

  display.display();
  display.setTextSize(2);

  index++;
  if (index >= 4)
    index = 0;
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
  Serial.printf("ID : %s, ", ID.c_str());
  Serial.printf("Pressure : %d PSI - %.2f bars, ", Pressure * 2, Pressure * 2 / 14.504);
  Serial.printf("Battery : %s, ", Batt);
  Serial.printf("Checksum : %s\n", (ChecksumCalc == ChesumMsg) ? "OK" : "NOK");
  Serial.flush();

  // Print data on SSD1306 screen
  if (ChecksumCalc == ChesumMsg)
  {
    display.fillRect(0, 0, 128, 48, BLACK);
    display.setCursor(0, 0);
    display.printf("ID: %s\n", ID.c_str());
    display.printf("P : %.2f\n", Pressure * 2 / 14.504);
    display.printf("B : %s\n", Batt);

    // Update the live indicator & time
    LiveIndicatorAndTime();
  }
}

void loop()
{

  long TimeFrame = micros();
  static bool NoComm = false;

  // Go to sleep after xx secods
  if (millis() - startMillis < TIME_TO_SLEEP)
  {

    // When button is pressed, raz of wake up timer & update time
    if (digitalRead(PIN_WAKE_UP) == LOW)
    {
      startMillis = millis();
      LiveIndicatorAndTime();
    }

    // Compute battery level of the receiver
    ComputeBatteryVoltage();

    // No high value during long time -> end of frame
    if ((TimeFrame - LastTime > TIME_END_FRAME) && (strlen((char *)bufferReception) > 0))
    {
      // Comm active as we received a frame
      NoComm = false;

      // Display the time
      Serial.printf("Time : %.1f, ", float(TimeFrame) / 1000000);

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

      display.fillRect(0, 48, 64, 16, BLACK);
      display.setCursor(0, 52);
      display.setTextSize(1);
      display.printf("No comm");
      display.display();
      display.setTextSize(2);

      // Init of variables
      frame = "";
      EmptyBuffer();

      // No comm
      NoComm = true;
    }
  }
  else
  {
    Serial.printf("Going to deep sleep\n");

    display.fillRect(0, 48, 128, 48, BLACK);
    display.setCursor(0, 48);
    display.printf("Sleep ...");
    display.display();

    sleep(5);

    goToSleep();
  }
}
