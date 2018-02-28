#ifndef HYDROSTOVE_DISPLAY_H
#define HYDROSTOVE_DISPLAY_H

// Compatibility with the Arduino 1.0 library standard
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>     //see https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h> //see https://github.com/adafruit/Adafruit_SSD1306

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif


#define CALOR_ESPECIF_AGUA  418    // J/K·kg


class HydroStoveDisplay {
  public:
    HydroStoveDisplay (int pinReset);
    unsigned int add(unsigned int tempIn, unsigned int tempOut, unsigned long flowRate);
    void refreshDisplay();

  private:
    Adafruit_SSD1306 _display;
    unsigned int _bufferIndex = 0;
    unsigned int _buffer[SSD1306_LCDWIDTH];
    unsigned int _currentTempIn, _currentTempOut, _currentFlowRate;
    unsigned int _scale = 1;
    unsigned int _maxValue = 0;

};

#endif  // HYDROSTOVE_DISPLAY_H
