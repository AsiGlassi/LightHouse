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

const int ledPhaseBy = 30;
const int ringLength = STRIPSIZE - 1;
const int actualResolution = 180;

// #define LB2KG 0.45352
const float knownMass = 2.0f;
const float weightThreashold = 10.0f;

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
uint32_t colorArr[actualResolution];

const int HX711_dout = 5; // mcu > HX711 dout pin, must be external interrupt capable!
const int HX711_sck = 4;  // mcu > HX711 sck pin
HX711_ADC LoadCell(HX711_dout, HX711_sck);
unsigned long lastTime = 0;
unsigned int serialPrintInterval = 1500; // increase value to slow down serial print activity
boolean arrayLightyInitialized = false;

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
  arrayLightyInitialized = false;
  lastColor = origColor;

  //Sacle Init
  float calibrationValue = -25272.00f;  
  long offsetValue = 8373663;

  LoadCell.begin();

  Serial.print("Offset = "); Serial.println(offsetValue);
  LoadCell.setTareOffset(offsetValue);
  //LoadCell.setReverseOutput();
  unsigned long stabilizingtime = 1000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  LoadCell.start(stabilizingtime, false);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Timeout!");//, check HX711 wiring and pin"); 
    SetErrorState();
  } else {
    LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
    Serial.println("Startup is completed");
  }
  
  int eeAddress = 0;
  byte calibInfoExist = 0x01;

  //try to get calibration info from eprom
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(512); 
#endif
  EEPROM.get(eeAddress, calibInfoExist);
  Serial.print("calibInfoExist "); Serial.println(calibInfoExist); 
  eeAddress += sizeof(byte);
  if (calibInfoExist != 0x01) {

    //Configuration Not Exist --> Set Cal Mode
    strip.fill((strip.gamma32(calP1Color)));
    Serial.println("No Data in EEPROM --> Calibration Starts, Remove load !");
    delay(5000);

    //Phase 1 - set to Zero
    LoadCell.tareNoDelay(); // calculate the new tare / zero offset value (blocking)
    offsetValue = LoadCell.getTareOffset(); // get the new tare / zero offset value

    //Phaase 2 - measure 3kg weight
    strip.fill(strip.gamma32(calP2Color));
    Serial.println("Put a known Weight - 2kg !");
    delay(5000);

    //get the new calibration value
    LoadCell.refreshDataSet();  //refresh the dataset to be sure that the known mass is measured correct
    // calibrationValue = LoadCell.getNewCalibration(knownMass); 

    //save data in eeprom
    Serial.println("Writing new Data to EEPROM:");
    Serial.print("Offset = "); Serial.println(offsetValue);
    Serial.print("Calibration Factor = "); Serial.println(calibrationValue, 3);
        
    EEPROM.put(eeAddress, calibInfoExist);                // Cal exist
    eeAddress += sizeof(byte);

    EEPROM.put(eeAddress, offsetValue);                   // New offest - uint32_t
    eeAddress += sizeof(uint32_t);
    
    EEPROM.put(eeAddress, calibrationValue);              // New Cal - float
    eeAddress += sizeof(float);

#if defined(ESP8266) || defined(ESP32)
    EEPROM.commit();
#endif
  }

  eeAddress = 0;
  EEPROM.get(eeAddress, calibInfoExist);      // Cal exist
  // Serial.println(calibInfoExist);
  eeAddress += sizeof(byte); 

  EEPROM.get(eeAddress, offsetValue);         // New offest - uint32_t
  // Serial.println(offsetValue);  
  eeAddress += sizeof(uint32_t);
  
  EEPROM.get(eeAddress, calibrationValue);    // New New Cal - float
  // Serial.println(calibrationValue, 3 );  
  eeAddress += sizeof(float); 

  Serial.print("Read Offset = "); Serial.println(offsetValue);
  LoadCell.setTareOffset(offsetValue);
  Serial.print("Read Calibration Factor = "); Serial.println(calibrationValue, 3 );
  LoadCell.setCalFactor(calibrationValue);
  
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
  Serial.println("Startup is completed");
  delay(50);
  lastTime = millis();
}


void loop() {

  // Serial.print(".");
  boolean newDataReady = false;
  boolean statusChanged = false;

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > lastTime + serialPrintInterval) {
      float weight = LoadCell.getData();
      Serial.print("Weight: "); Serial.println(weight, 2);
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
  int ratio = 360/actualResolution;
  // Serial.println(ratio);
  if (statusChanged) {
    Serial.println("Status Changed.");

    uint16_t curr_r, curr_g, curr_b; // separate into RGB components
    curr_b = origColor & 0x00FF;
    curr_g = (origColor >> 8) & 0x00FF;
    curr_r = (origColor >> 16) & 0x00FF;

    // Set array with one cycle color
    for (int indx = 0; indx < actualResolution; indx++) {
      float sinRes = sin(((indx*ratio) - 90) * PI / 180);
      if (sinRes < 0) {sinRes = 0;}
      uint32_t newColor = (uint32_t)(sinRes * curr_b) + ((uint32_t)(sinRes * curr_g) << 8) + ((uint32_t)(sinRes * curr_r) << 16);
      // Serial.print(indx); Serial.print(" "); Serial.println(newColor);
      colorArr[indx] = newColor;
    }
    arrayLightyInitialized=true;
  }

  // Start lightning
  if (arrayLightyInitialized) {
    Serial.print("Start lightning :"); Serial.println(origColor, HEX);
    strip.setPixelColor(STRIPSIZE-1, strip.gamma32(origColor));
    for (int indx = 0; indx < actualResolution; indx=indx+1) {
      int runningIndx = indx;
      for (int led=0; led < STRIPSIZE-1; led++) {
        uint32_t newColor = colorArr[runningIndx];
        // Serial.print(runningIndx); Serial.print(" ");
        strip.setPixelColor(led, strip.gamma32(newColor));
        runningIndx += ledPhaseBy;
        runningIndx = runningIndx % (actualResolution-1);
      }
      strip.show();
      // Serial.println();
      delay(25);
    }
  }
  // Serial.println();
}
