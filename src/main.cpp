#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "HX711_ADC.h"
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
#define debug false

#define DATAPIN A0
#define STRIPSIZE 6

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIPSIZE, DATAPIN, NEO_GRB + NEO_KHZ800);

uint32_t redColor = (0xAE0649);
uint32_t greenColor = (0x33cc33);

const int HX711_dout = 5; //mcu > HX711 dout pin, must be external interrupt capable!
const int HX711_sck = 4; //mcu > HX711 sck pin
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const int calVal_eepromAdress = 0;
unsigned long t = 0;


//set all pixel to the same color
void SetFillColor(uint32_t color) {
    strip.fill((strip.gamma32(color)));
    strip.show(); 
}

void setup() {
  Serial.begin(57600);
  Serial.println("Or Light House Setup");

  strip.begin();
  strip.setBrightness(75);  // Lower brightness and save eyeballs!
  strip.show();             // Initialize all pixels to 'off'}

  float calibrationValue; // calibration value
  calibrationValue = 696.0; // uncomment this if you want to set this value in the sketch
#if defined(ESP8266) || defined(ESP32)
  // EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom
#endif
  // EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch this value from eeprom

  LoadCell.begin();
  LoadCell.setReverseOutput();
  unsigned long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
  Serial.print("Calibration value: ");
  Serial.println(LoadCell.getCalFactor());
  Serial.print("HX711 measured conversion time ms: ");
  Serial.println(LoadCell.getConversionTime());
  Serial.print("HX711 measured sampling rate HZ: ");
  Serial.println(LoadCell.getSPS());
  Serial.print("HX711 measured settlingtime ms: ");
  Serial.println(LoadCell.getSettlingTime());
  Serial.println("Note that the settling time may increase significantly if you use delay() in your sketch!");
  if (LoadCell.getSPS() < 7) {
    Serial.println("!!Sampling rate is lower than specification, check MCU>HX711 wiring and pin designations");
  }
  else if (LoadCell.getSPS() > 100) {
    Serial.println("!!Sampling rate is higher than specification, check MCU>HX711 wiring and pin designations");
  }
}


void loop() {
    //Serial.println("Or Light House Loop");
    static uint32_t origColor = (0x1AB2E7);

    static boolean newDataReady = 0;
    const int serialPrintInterval = 500; //increase value to slow down serial print activity
    
    // check for new data/start next conversion:
    if (LoadCell.update()) newDataReady = true;

    // get smoothed value from the dataset:
    if (newDataReady) {
      if (millis() > t + serialPrintInterval) {
        float i = LoadCell.getData();
        Serial.print("Load_cell output val: ");
        Serial.println(i);
        if (i > 200) {
          origColor = redColor;
        } else {
          origColor = greenColor;
        }
        newDataReady = 0;
        t = millis();
      }
    }

    // // receive command from serial terminal, send 't' to initiate tare operation:
    // if (Serial.available() > 0) {
    //   char inByte = Serial.read();
    //   if (inByte == 't') LoadCell.tareNoDelay();
    // }

    // // check if last tare operation is complete:
    // if (LoadCell.getTareStatus() == true) {
    //   Serial.println("Tare complete");
    // }

    uint8_t step = 10;
    int phaseBy = 95;
    int ringLength = STRIPSIZE-1;

    strip.setPixelColor(STRIPSIZE-1, strip.gamma32(origColor));
    uint16_t curr_r, curr_g, curr_b;
    curr_b = origColor & 0x00FF; curr_g = (origColor >> 8) & 0x00FF; curr_r = (origColor >> 16) & 0x00FF;  // separate into RGB components

    int totalSteps = (ringLength - 1) * phaseBy + 360;
    for (int it = 0; it < totalSteps; it=it+step)
    {
        for (int led = 0; led < ringLength; led++)
        {
          int ledVal = it - led*phaseBy;
          if (ledVal < 0) 
          {
            ledVal = 0;
          } else if (ledVal > 360) {
            ledVal = 360;
          }
          
          float sinRes = (sin((ledVal-90)*PI/180) + 1) /2;
          // Serial.printf("%d-%f \t R: %d\tG: %d\tB:%d\n", deg, sinRes, (uint16_t)(sinRes * curr_r), (uint16_t)(sinRes * curr_g), (uint16_t)(sinRes * curr_b));   
          uint32_t newColor = (uint32_t)(sinRes * curr_b) + ((uint32_t)(sinRes * curr_g) << 8) + ((uint32_t)(sinRes * curr_r) << 16);
          strip.setPixelColor(led, strip.gamma32(newColor));
          strip.show();
        
          delay(2);
        }  
    }
}