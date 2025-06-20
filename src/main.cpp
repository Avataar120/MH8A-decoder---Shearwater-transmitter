#include <Arduino.h>

#define ADC_PIN 4        // GPIO4
#define ADC_THRESHOLD 300 // 80mV

#define TIME_MIN_0 1100       // us
#define TIME_MAX_0 2300       // us
#define TIME_MIN_1 TIME_MAX_0 // us
#define TIME_MAX_1 5000       // us
#define TIME_END_FRAME 15000  // us
#define TIMEOUT 8000000       // us

static String frame = "";

typedef enum
{
  waitForBit,
  receiveBit,
  decodeFrame,
  waitForFrame
} tReceptionState;

void setup()
{

  // Serial init
  Serial.begin(115200);
  delay(500);

  // Set ADC resolution at the minimum to go faster
  // Set attenuation at 0db as signal is very low (100mV)
  analogReadResolution(4);
  analogSetAttenuation(ADC_0db);

  // Reserve 64 bytes to avoid alloc / dealloc each time
  frame.reserve(64);

  Serial.println("Init succesfull");
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
void Decode(String frameToBeDecoded)
{
  // Display the raw frame
  // Serial.println(frameToBeDecoded);

  // If length is not the good one, exit
  if (frameToBeDecoded.length() != 58)
  {
    Serial.printf("NOK, ");
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
  Serial.printf("Checksum : %s, ", (ChecksumCalc == ChesumMsg) ? "OK" : "NOK");
}

void loop()
{
  static tReceptionState state = waitForBit;
  static uint32_t start = 0;

  // Used to detect the strength of the signal
  static uint32_t SumPeaks = 0;
  static uint32_t NbPeaks = 0;

  // Read ADC value and decide if high level
  int adcValue = analogReadMilliVolts(ADC_PIN);
  bool Peak = (adcValue > ADC_THRESHOLD);

  // Get current time in µs + calculate the delta from last call
  uint32_t time = micros();
  uint32_t delta = time - start;

  // This state machine will read each bit of the signal
  // Sinusoid 38khz during 1ms + Short pause 1ms is a 0
  // sinusoid 38khz during 1ms + long pause 2ms is a 1
  // Purpose is to measure the time between the first high level and then to wait the pause to get the next high level
  // Normaly 0 is then 1ms sinusoid + 1 ms pause
  // and 1 is 1ms sinusoid + 2ms pause
  switch (state)
  {

    // In this state, we are waiting for a high level on the ADC pin - we update continuoulsy the start of the timer
  case waitForBit:
    start = time;

    if (Peak)
      state = receiveBit;

    break;

    // Here we do not touch the timer start value
    // we look at the next high level - if short time -> we discard as we are still in the sinusoid at 38khz
    // if between range of 0 -> 0
    // if between range of 1 -> 1
    // if long time without signal -> frame is over, ready to be decoded
  case receiveBit:

    // If high level detected
    if (Peak)
    {
      // Calculate average of signal strentgh for this frame
      SumPeaks += adcValue;
      NbPeaks++;

      // 0 detection
      if ((delta > TIME_MIN_0) && (delta <= TIME_MAX_0))
      {
        frame += "0";
        state = waitForBit;
      }
      // 1 detection
      else if ((delta > TIME_MIN_1) && (delta <= TIME_MAX_1))
      {
        frame += "1";
        state = waitForBit;
      }
    }

    // No high value during long time -> end of frame
    if (delta > TIME_END_FRAME)
      state = decodeFrame;

    break;

    // Frame reception is over -> time to decode it and display it
  case decodeFrame:
    // Display the time
    Serial.printf("Time : %.1f, ", float(start) / 1000000);

    // Decode the frame and display it on the console
    Decode(frame);

    // display the stats of signal strentgh
    Serial.printf("Average mV : %f\n", (float)SumPeaks / NbPeaks);

    // Init of variables
    frame = "";
    SumPeaks = 0;
    NbPeaks = 0;

    // Wait from new frame state
    state = waitForFrame;
    break;

    // Wait for new frame or end of communication if tank is closed / pressure removed from regulator - Com is stopping after 2min
  case waitForFrame:

    // If high level detected -> system is still alive, we start new reading (a new frame is sent every 5s)
    if (Peak)
    {
      start = time;
      state = receiveBit;
    }
    // If no communication for several seconds -> no more communication
    else if (time - start > TIMEOUT)
    {
      Serial.printf("No more communication\n");
      state = waitForBit;
    }
    break;
  }
}
