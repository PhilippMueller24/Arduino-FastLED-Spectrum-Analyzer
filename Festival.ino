// This code was mainly programmed by Daniel Perez, E-Mail: generatorlabs@gmail.com and adapted to Sparkfuns Spectrum Shield using the MSGEQ7 band-slicing IC's 


#include <FastLED.h>            // You must include FastLED version 3.002.006. This library allows communication with each LED
#define DATA_PIN 22             // Pin for serial communication with LED string.
#define STROBE_PIN 4            // Pin to instruct MSGEQ7 IC's to inspect next band (band 0 thru 6).
#define RESET_PIN 5             // Pin to instruct MSGEQ7 to return to band zero and inspect it. 
#define COLUMN 14               // Number of columns in LED project. (14)
#define ROWS 21                 // Number of rows (left to right) in LED project. (21)
#define NUM_LEDS COLUMN * ROWS  // Total number of LED's in the project.
#define LEDTYPE WS2813         // Type of LED communication protocol used. 
#define BRIGHTNESS  50          // Intensity of LED's. The lower the number the longer LED's will last. LED's do have a finite life span when run hard.
// It is strongly recommended to keep this number as low as possible. Inexpensive LED strips will have a noticeably shorter life and pull large
// amounts of current unecessarily. Large surges in current could lead to current starvation and possibly erratic operation. Your power supply must be
// sized correctly. A string of 300 LED's could potentially require a 18 amp power supply! Avoid drawing white as a color if your power supply is 
// substandard or poorly regulated. For reference, my 294 LED analyzer, with a brightness of 50 and static rainbow columns will average less than
// 0.5 amps @ 5vdc. If you drive the LED's conseratively you will get good results.

// Noise-floor compensator. Set this number to eliminate noise picked up by circuit. When watching serial monitor, data
// should be closer to zero with an audio source connected and no music playing.
#define NOISECOMP 150   // 120 is a good start point. ; My number is 160   

// Use a frequency generator app on a smart phone to adjust BOOST. All led's should in selected band should light up when sweeping frequencies 
// at high volume.
// Boost works similar to the Arduino "constrain" function but is easier to dial in additional boost for weak input signals.
// Test on 63Hz, 160Hz, 400Hz, & 1000Hz.
#define BOOST 6

// Matrix Definition
CRGB leds[NUM_LEDS];      // Setup memory block and array for all LED's
typedef struct ledrgb     // Structure defining the parameters related to each led
{
  int hue;
  int sat;
  int val;
  int nled;
  boolean active;
} led;
led colors[COLUMN][ROWS];   // Matrix containing the values of the structure variables.

//Global Variables
int MSGEQ_Bands[COLUMN];    // Setup column array to store instantaneous sample of each band.
byte DELTA;                 // Variable use to affect scaling of each column
int nlevel;                 // Level index
int hue_rainbow = 0;        // Global variable for the rainbow variable hue.
int long rainbow_time = 0;
int long time_change = 0;
int effect = 2;             // Load this color effect on startup
int n = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////------------------- SETUP ----------------------////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  // Serial.begin(57600);      // Enable this for serial monitor or troubleshooting

  pinMode(DATA_PIN, OUTPUT);
  pinMode(STROBE_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);

  DELTA = 1024 / ROWS - BOOST;      // Do not change this line. Adjust BOOST, defined above, to effect gain

  int count = 0;
  for (int i = 0; i < COLUMN; i++)          //Sequentially number the leds
  {
    for (int j = 0; j < ROWS; j++)
    {
      colors[i][i % 2 == 0 ? j : ROWS-j-1].nled = count; // invert every second column
      count++;
    }
  }

  FastLED.addLeds<LEDTYPE, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  rainbow_time = millis();
  time_change = millis();
}


void loop()
{
  readMSGEQ7();                                  // Call to function that reads MSGEQ7 IC's via analogue inputs.

  if (millis() - time_change > 3000)             // Code that establishes how often to change effect. 1000 = 1 Second
  {
    //effect = 2;                                  // Enable this line to set a fixed mode
    effect++;                                  // Enable this line to cycle through different modes
    if (effect > 7)
    {
      effect = 0;
    }
    time_change = millis();
  }


  switch (effect)                                // Case logic to determine which color effect to use
  {
    case 0:                                      // Full column; each band different color; color gradient within each band
      rainbow_dot();
      full_column();
      updateHSV();
      break;

    case 1:                                      // Full column; each band the same color; gradual simultaneous color change across all bands
      if (millis() - rainbow_time > 15)
      {
        dynamic_rainbow();
        rainbow_time = millis();
      }
      full_column();
      updateHSV();
      break;

    case 2:                                      // Full column; each band a different static rainbow color for the specified interval
      if (millis() - rainbow_time > 600)
      {
        rainbow_column();
        rainbow_time = millis();
      }
      full_column();
      updateHSV();
      break;

    case 3:                                      // Full column; all bands same static color
      if (millis() - rainbow_time > 15)
      {
        total_color_hsv(255, 255, 255);
        rainbow_time = millis();
      }
      full_column();
      updateHSV();
      break;

    case 4:                                      // Dot column; each column a different static rainbow color
      if (millis() - rainbow_time > 15)
      {
        rainbow_dot();
        rainbow_time = millis();
      }
      full_column_dot();
      updateHSV();
      break;
    case 5:                                      // Dot column; each band the same color; gradual simultaneous color change across all bands
      if (millis() - rainbow_time > 15)
      {
        dynamic_rainbow();
        rainbow_time = millis();
      }
      full_column_dot();
      updateHSV();
      break;

    case 6:                                      // Dot column; each band a different static rainbow color
      if (millis() - rainbow_time > 15)
      {
        rainbow_column();
        rainbow_time = millis();
      }
      full_column_dot();
      updateHSV();
      break;

    case 7:                                      // Dot column; all bands same static color
      total_color_hsv(55, 255, 255);
      full_column_dot();
      updateHSV();
      break;
  }
  delay(30);                                     // Refresh rate; Values 20 thru 30 should look realistic
}

void readMSGEQ7(void)                               // Function that reads the 7 bands of the audio input.
{
  //digitalWrite(STROBE_PIN, HIGH);                 // Make sure Strobe line is low before entering loop.

  digitalWrite(RESET_PIN, HIGH);                    // Part 1 of Reset Pulse. Reset pulse duration must be 100nS minimum.
  digitalWrite(RESET_PIN, LOW);                     // Part 2 of Reset pulse. These two events consume more than 100nS in CPU time.

  for (int band = 0; band < COLUMN; band++) {       // Loop that will increment counter that AnalogRead uses to determine which band to store data for.
    digitalWrite(STROBE_PIN, LOW);                  // Re-Set Strobe to LOW on each iteration of loop.
    delayMicroseconds(30);                          // Necessary delay required by MSGEQ7 for proper timing.
    MSGEQ_Bands[band] = analogRead(0) - NOISECOMP;  // Saves the reading of the amplitude voltage on Analog Pin 0.
    // Serial.print(band);
    // Serial.print("  ");
    // Serial.print(MSGEQ_Bands[band]);
    // Serial.print("  ");
    band++;
    MSGEQ_Bands[band] = analogRead(1) - NOISECOMP;  // Saves the reading of the amplitude voltage on Analog Pin 1.
    // Serial.print(band);
    // Serial.print("  ");
    // Serial.print(MSGEQ_Bands[band]);
    // Serial.print("  ");

    digitalWrite(STROBE_PIN, HIGH);
  }
  // Serial.println();
}

void updateHSV(void)
{

  int total_level = 0;
  
  for (int i = 0; i < COLUMN; i++) {
    for (int j = 0; j < ROWS; j++) {
      if (colors[i][j].active == 1) {
        total_level += j;
        leds[colors[i][j].nled] = CHSV(colors[i][j].hue, colors[i][j].sat, colors[i][j].val);
      } else {
        leds[colors[i][j].nled] = CRGB::Black;
      }

    }
  }

  // uncomment this line to enable brightness based on avg level
  //FastLED.setBrightness( (total_level / ROWS)*10 );
  
  FastLED.show();
}

void full_column(void)
{
  nlevel = 0;
  for (int i = 0; i < COLUMN; i++) {
    nlevel = MSGEQ_Bands[i] / DELTA;
    for (int j = 0; j < ROWS; j++) {
      if (j <= nlevel) {
        colors[i][j].active = 1;
      }
      else {
        colors[i][j].active = 0;
      }
    }
  }
}

void full_column_dot(void)
{
  nlevel = 0;
  for (int i = 0; i < COLUMN; i++) {
    nlevel = MSGEQ_Bands[i] / DELTA;
    for (int j = 0; j < ROWS; j++) {
      if (j == nlevel) {
        colors[i][j].active = 1;
      }
      else {
        colors[i][j].active = 0;
      }
    }
  }
}

void total_color_hsv(int h, int s, int v)
{
  for (int i = 0; i < COLUMN; i++) {
    for (int j = 0; j < ROWS; j++) {
      colors[i][j].hue = h;
      colors[i][j].sat = s;
      colors[i][j].val = v;
    }
  }
}

void rainbow_column(void)
{
  //int n = 18;
  for (int i = 0; i < COLUMN; i++) {
    for (int j = 0; j < ROWS; j++) {
      colors[i][j].hue = n;
      colors[i][j].sat = 230;
      colors[i][j].val = 240;
    }
    n += 18;  //36 For 7 Columns
  }
}

void rainbow_dot(void)
{
  int n = 36;
  for (int i = 0; i < COLUMN; i++) {
    for (int j = 0; j < ROWS; j++) {
      colors[i][j].hue = n;
      colors[i][j].sat = 230;
      colors[i][j].val = 240;
      n += 5;
    }
  }
}

void dynamic_rainbow(void)
{
  for (int i = 0; i < COLUMN; i++) {
    for (int j = 0; j < ROWS; j++) {
      colors[i][j].hue = hue_rainbow;
      colors[i][j].sat = 230;
      colors[i][j].val = 240;
    }
  }
  hue_rainbow++;
}
