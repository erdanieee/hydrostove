#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <HydroStoveDisplay.h>


#define LCD_YELLOW  16


// Inicializa el display y variables
HydroStoveDisplay::HydroStoveDisplay(int pinReset) : _display(pinReset) {
  //clear buffer
  for (int i=0; i<SSD1306_LCDWIDTH; i++){
    _buffer[i] = 0;
  }
}


/**
  Añade un nuevo valor al buffer (potencia instantánea) y refresca la pantalla
  Parámetros:
  tempIn: temperatura de entrada al sistema
  tempOut: temperatura de salida del sistema
  flowRate: caudal en l/s

  Return: valor de reescalado. Si se desea mantener la escala temporal en la gráfica,
          el periodo debe multiplicarse por este valor. Es decir, si al inicio
          se incorpora un valor cada 5 segundos, con un reescalado de 2 deberán
          incorporarse cada 10, 3 cada 15 y así sucesivamente.
  **/
unsigned int HydroStoveDisplay::add(unsigned int tempIn, unsigned int tempOut, unsigned long flowRate){
  unsigned int power;

  if (_bufferIndex >= SSD1306_LCDWIDTH){
    unsigned int i=0, j=0;
    _scale++;

    //comprime los valores a la mitad del buffer
    while (i<_bufferIndex){
      _buffer[j++] = floor( (_buffer[i++] + _buffer[i++])/2 );
    }
    _bufferIndex = j;
  }

  //Añade un nuevo valor de potencia instantánea al buffer
  power = CALOR_ESPECIF_AGUA * flowRate * (tempOut-tempIn);
  _buffer[_bufferIndex++] = power;

  if (power > _maxValue){
    _maxValue = power;
  }

  _currentTempIn    = tempIn;
  _currentTempOut   = tempOut;
  _currentFlowRate  = flowRate;
  refreshDisplay();

  return _scale;
}


void HydroStoveDisplay::refreshDisplay(){
  if (_bufferIndex <=0){
    return;
  }

  _display.clearDisplay();
  _display.setTextSize(1);
  _display.setTextColor(WHITE);
  _display.setCursor(0,0);
  _display.println(String(_currentTempIn) + " ºC " +
                   String(_currentTempOut) + " ºC " +
                   String(_currentFlowRate*3600) + " l/h" +
                   String(_buffer[_bufferIndex-1]) + " W");

  for (uint8_t x=0; x<= _bufferIndex; x++){
    int16_t y;

    y = floor(_buffer[x]*16 / _maxValue);
    _display.drawLine(x, y, x, SSD1306_LCDHEIGHT-1, WHITE);
  }


}
