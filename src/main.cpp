#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "HX711_ADC.h"
#if defined(ESP8266) || defined(ESP32) || defined(AVR)
// #include <EEPROM.h>
#endif
#define debug false

boolean inErrorState = false;

#define DATAPIN A0
#define STRIPSIZE 6

const uint8_t step = 5;
const int phaseBy = 60;
const int ringLength = STRIPSIZE - 1;
const int resolution = 360;
const int weightThreashold = 50;

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIPSIZE, DATAPIN, NEO_GRB + NEO_KHZ800);

uint32_t redColor = (0xAE0649);
uint32_t greenColor = (0x33CC33);
uint32_t errorColor = (0xAA0000);
uint32_t origColor = (0x1AB2E7);
uint32_t lastColor = origColor;

const int HX711_dout = 5; // mcu > HX711 dout pin, must be external interrupt capable!
const int HX711_sck = 4;  // mcu > HX711 sck pin
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const int calVal_eepromAdress = 0;
unsigned long lastTime = 0;

// set all pixel to the same color
void SetFillColor(uint32_t color) {
  strip.fill((strip.gamma32(color)));
  strip.show();
}

void SetErrorState (){
  inErrorState = true;
  SetFillColor(errorColor);
  delay(500);
  exit(-1);
}

void setup() {
  Serial.begin(57600);
  Serial.println("Or Light House");

  strip.begin();
  strip.setBrightness(75); // Lower brightness and save eyeballs!
  SetFillColor(origColor);
  strip.show();            // Initialize all pixels to 'off'}

  float calibrationValue;   // calibration value
  calibrationValue = 696.0; // uncomment this if you want to set this value in the sketch
#if defined(ESP8266) || defined(ESP32)
  // EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom
#endif
  // EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch this value from eeprom

  LoadCell.begin();
  LoadCell.setReverseOutput();
  unsigned long stabilizingtime = 1000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true;                 // set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout!");//, check HX711 wiring and pin"); 
    SetErrorState();
  } else {
    LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
    Serial.println("Startup is completed");
  }
  while (!LoadCell.update());
  // Serial.print("Cal val: ");
  // Serial.println(LoadCell.getCalFactor());
  // Serial.print("HX711 conv in ms: ");
  // Serial.println(LoadCell.getConversionTime());
  // Serial.print("HX711 SPS rate HZ: ");
  // Serial.println(LoadCell.getSPS());
  // Serial.print("HX711 settling in ms: ");
  // Serial.println(LoadCell.getSettlingTime());
  // // Serial.println("Note that the settling time may increase significantly if you use delay() in your sketch!");
  if (LoadCell.getSPS() < 7) {
    Serial.print("Sampling rate < spec!");//, check HX711 wiring and pin"); 
    SetErrorState();
  } else if (LoadCell.getSPS() > 100) {
    Serial.print("Sampling rate > spec!");//, check HX711 wiring and pin"); 
    SetErrorState();
  }
  delay(50);
}


void loop() {

  uint32_t colorArr[resolution];

  boolean newDataReady = false;
  unsigned int serialPrintInterval = 1500; // increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  boolean statusChanged = false;
  if (newDataReady) {
    if (millis() > lastTime + serialPrintInterval) {
      float weight = LoadCell.getData();
      Serial.print("Weight: "); Serial.println(weight);
      if (weight > weightThreashold) {
        origColor = redColor;
      } else {
        origColor = greenColor;
      }
      lastTime = millis();

      statusChanged = (lastColor != origColor);
      lastColor = origColor;
    }
  }

  //rebuild color table
  if (statusChanged) {
    Serial.println("S C.");

    uint16_t curr_r, curr_g, curr_b; // separate into RGB components
    curr_b = origColor & 0x00FF;
    curr_g = (origColor >> 8) & 0x00FF;
    curr_r = (origColor >> 16) & 0x00FF;

    // Set array with one cycle color
    for (int indx = 0; indx < resolution; indx++) {
      float sinRes = sin((indx - 90) * PI / 180);
      if (sinRes < 0) {sinRes = 0;}
      uint32_t newColor = (uint32_t)(sinRes * curr_b) + ((uint32_t)(sinRes * curr_g) << 8) + ((uint32_t)(sinRes * curr_r) << 16);
      colorArr[indx] = newColor;
    }
  }

  //Start lightning
  strip.setPixelColor(STRIPSIZE - 1, strip.gamma32(origColor));
  int ledPhase = phaseBy; //resolution / (STRIPSIZE - 1);
  for (int indx = 0; indx < resolution; indx=indx+step) {
    int runningIndx = indx;
    for (int led=0; led < STRIPSIZE-1; led++) {
      uint32_t newColor = colorArr[runningIndx];
      // Serial.print(runningIndx); Serial.print(" ");
      strip.setPixelColor(led, strip.gamma32(newColor));
      runningIndx += ledPhase;
      runningIndx = runningIndx % resolution;
    }
    strip.show();
    Serial.println();
    delay(25);
  }
  // Serial.println();
}
