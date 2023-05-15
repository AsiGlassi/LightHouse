#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "HX711_ADC.h"
#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
#define debug false

boolean inErrorState = false;

#define DATAPIN A0
#define STRIPSIZE 6

const uint8_t step = 5;
const int phaseBy = 60;
const int ringLength = STRIPSIZE - 1;
const int resolution = 360;

// #define LB2KG 0.45352
const float knownMass = 2000;
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
uint32_t calP1Color = (0xFFFF00);
uint32_t calP2Color = (0xFFFFAA);

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

void SetErrorState () {
  inErrorState = true;
  SetFillColor(errorColor);
  delay(500);
  exit(-1);
}

void setup() {
  Serial.begin(57600);
  Serial.println("Or Light House");

  //Led init
  strip.begin();
  strip.setBrightness(75); // Lower brightness and save eyeballs!
  SetFillColor(origColor);
  strip.show();            // Initialize all pixels to 'off'}

  //Sacle Init
  LoadCell.begin();
  LoadCell.setReverseOutput();
  unsigned long stabilizingtime = 1000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  LoadCell.start(stabilizingtime, false);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout!");//, check HX711 wiring and pin"); 
    SetErrorState();
  }

  byte calInfoExist = 0x00;
  uint32_t calibrationOffset = 0x000000;   // offset value
  float calibrationValue = 696.0;   // calibration value
  LoadCell.setCalFactor(calibrationValue);

  //try to get calibration info from eprom
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom
#endif
  int read_eepromAdress = calVal_eepromAdress;
  EEPROM.get(read_eepromAdress, calInfoExist); 
  if (calInfoExist != 0x01) {

    //Configuration Not Exist --> Set Cal Mode
    strip.fill((strip.gamma32(calP1Color)));
    Serial.println("No Data in EEPROM --> Calibration Starts, Remove load !");
    delay(10000);

    //Phase 1 - set to Zero
    LoadCell.tareNoDelay(); // calculate the new tare / zero offset value (blocking)
    calibrationOffset = LoadCell.getTareOffset(); // get the new tare / zero offset value

calibrationOffset=0x112233;

    //Phaase 2 - measure 3kg weight
    strip.fill((strip.gamma32(calP2Color)));
    Serial.println("Put a known Weight - 2kg !");
    delay(10000);

    //get the new calibration value
    LoadCell.refreshDataSet();  //refresh the dataset to be sure that the known mass is measured correct
    float newCalibrationValue = LoadCell.getNewCalibration(knownMass); 

    //save data in eeprom
    Serial.println("Writing new Data to EEPROM:");
    Serial.print("Offset = "); Serial.println(calibrationOffset);
    Serial.print("Calibration Factor = "); Serial.println(calibrationValue);

    int write_eepromAdress = calVal_eepromAdress;
    EEPROM.put(write_eepromAdress, 0x01);                 // Cal exist
    read_eepromAdress += 0x01;
    EEPROM.put(write_eepromAdress, calibrationOffset);    // New tare offest - uint32_t
    read_eepromAdress += sizeof(uint32_t);
    EEPROM.put(write_eepromAdress, newCalibrationValue);  // New Cal - float
#if defined(ESP8266) || defined(ESP32)
    EEPROM.commit();
#endif
  
    LoadCell.setTareOffset(calibrationOffset); 
    LoadCell.setCalFactor(newCalibrationValue);
  }

  read_eepromAdress += 0x01;
  EEPROM.get(read_eepromAdress, calibrationOffset); // New tare offest - uint32_t 
  read_eepromAdress += sizeof(uint32_t);
  EEPROM.get(read_eepromAdress, calibrationValue);  // New Cal - float 
  Serial.println("Read Offset = " + String(calibrationOffset));
  Serial.println("Read Calibration Factor = " + String(calibrationValue));
  LoadCell.setTareOffset(calibrationOffset);
  LoadCell.setCalFactor(calibrationValue);
  Serial.println("Startup is completed");
  
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
    Serial.println("Status C.");

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
    // Serial.println();
    delay(25);
  }
  // Serial.println();
}
