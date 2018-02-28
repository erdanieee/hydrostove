/*********************************************************************
Programa del controlador de HydroStove.
El programa básicamente va recopilando información de 2 sensores de
temperatura colocados justo a la entrada y a la salida del intercambiador
de la chimenea, y de un caudalímetro digital. Con esos datos, muestra los
valores actuales de temperatura y potencia instantánea, así como un
gráfico con la pontencia desarrollada en las últimas horas. Al mismo
tiempo, si la temperatura es muy alta a la salida del intercambiador, o
se detecta una detención del flujo de agua, se muestra un mensaje de aviso
y se activa una alarma sonora (silenciable por el usuario).

Flujo del programa.
Setup:
  inicializa serial, pantalla, pines, filtros, variables, ...

Loop:
  * lee temperatura salida
  * lee temperatura entrada
  * lee caudalímetro
  * Si la temperatura de salida es muy alta o el flujo nulo:
    - muestra en pantalla los mensajes de aviso
    - activa alarma sonora si es un nuevo aviso
  * Mostrar pantalla normal
    - temperatura de entrada
    - temperatura de salida
    - potencia actual
    - potencia acumulada
    - gráfica de potencia actual desde que se encendió la chimenea


*********************************************************************/
#include <Arduino.h>
#include <SignalFilter.h>     //see https://github.com/jeroendoggen/Arduino-signal-filtering-library
#include <FlowMeter.h>        //see https://github.com/sekdiy/FlowMeter
#include <main.h>
#include <HydroStoveDisplay.h>


//Define pin in/out
#define PIN_SERIAL_RX     0                         //hardware
#define PIN_SERIAL_TX     1                         //hardware
#define PIN_FLOWMETER     2                         //External interrupt (2,3)
#define PIN_OLED_RESET    3                         //IO OUT   (for OLED)
//#define PIN_I2C_SDA       4                         //hardware (for OLED)
//#define PIN_I2C_SCL       5                         //hardware (for OLED)
#define PIN_BUTTON_1      6                         //IO IN
#define PIN_TEMP_OUT      7                         //ADC IN
#define PIN_TEMP_IN       8                         //ADC IN
#define PIN_LED           13                        //hardware

//#define NUMFLAKES 10
//#define XPOS 0
//#define YPOS 1
//#define DELTAY 2

// set the measurement update period to 1s (1000 ms)
#define DELTA_FLOW    1000

// refresca la pantala a 2fps
#define DELTA_DISPLAY 500

#define SERIAL_RESISTOR_HOT   10000
#define SERIAL_RESISTOR_COLD   10000
#define THERMISTORNOMINAL    100000                // resistance at 25 degrees C
#define TEMPERATURENOMINAL   25                    // temp. for nominal resistance (almost always 25 C)
#define BCOEFFICIENT         3950                  // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR       98700                 // the value of the 'other' resistor





HydroStoveDisplay display(PIN_OLED_RESET);

FlowSensorProperties MySensor = {60.0f, 4.5f, {1.2, 1.1, 1.05, 1, 1, 1, 1, 0.95, 0.9, 0.8}}; //see https://github.com/sekdiy/FlowMeter/wiki/Calibration
FlowMeter Meter = FlowMeter(PIN_FLOWMETER, MySensor);

SignalFilter outSensor, inSensor;
int tempOut, tempIn;

unsigned long currentTime, lastFlowMeter, lastDisplay, lastBuffered;
volatile int adcAux;
unsigned int l_hour; // Calculated litres/hour






/*#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };
  */


/*
 * Función llamada cada vez que el caudalímetro produce un pulso.
 */
void flowISR (){ // Interrupt function
  Meter.count();
}


void setup()   {
  Serial.begin(9600);

  //Init pin
  pinMode(PIN_FLOWMETER, INPUT_PULLUP);
  pinMode(PIN_TEMP_OUT,  INPUT);
  pinMode(PIN_TEMP_IN,   INPUT);
  pinMode(PIN_BUTTON_1,  INPUT_PULLUP);
  pinMode(PIN_LED,       OUTPUT);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  //display.begin(SSD1306_SWITCHCAPVCC, 0x3D);  // initialize with the I2C addr 0x3D (for the 128x64)
  //display.clearDisplay();

  //Init temperature sensors filters
  outSensor.begin();
  outSensor.setFilter('m'); //bessel filter (b), median filter (m) or Chebyshev filter (c)
  //outSensor.setOrder(2);
  inSensor.begin();
  inSensor.setFilter('m');
  //inSensor.setOrder(2);

  attachInterrupt(0, flowISR, FALLING); // Setup Interrupt
  lastFlowMeter  = millis();
  // sometimes initializing the gear generates some pulses that we should ignore
  Meter.reset();
  sei(); // Enable interrupts

  //show logo
  delay(2000);

  lastDisplay=0;
  lastBuffered=0;
}


void loop() {
  //Lee temperatura de salida
  adcAux = analogRead(PIN_TEMP_OUT);
  tempOut = adc2temp(outSensor.run(adcAux), SERIAL_RESISTOR_HOT);
  Serial.println("OUT: " + String(tempOut) + " ºC (" + String(adcAux) + ")");

  //lee temperatura de entrada
  adcAux = analogRead(PIN_TEMP_IN);
  tempIn = adc2temp(inSensor.run(adcAux), SERIAL_RESISTOR_COLD);
  Serial.println("IN: " + String(tempIn) + " ºC (" + String(adcAux) + ")");

  //lee caudalímetro
  if (millis() - lastFlowMeter >= DELTA_FLOW){
    Meter.tick(millis() - lastFlowMeter);
    lastFlowMeter = millis();
    // output some measurement result
    Serial.println("FLOW: " + String(Meter.getCurrentFlowrate()) + " l/min, " + String(Meter.getTotalVolume())+ " l total.");
  }

  //valora los avisos


  //refresca la pantalla
  if (millis() - lastDisplay >= DELTA_DISPLAY){
    //refreshDisplay();
    lastDisplay = millis();
  }

}





/*
 * Refresca el display
*/
void refreshDisplay(){
  // Potencia instantánea (W) = calor específico agua  (J/Kg·K) * caudal (Kg/s) * deltaTemperatura (K)
  //pot = CALOR_ESPECIF_AGUA * Meter.getCurrentFlowrate() * (tempOut-tempIn)

  //display.clearDisplay();
  //display.setTextSize(1);
  //display.setTextColor(WHITE);
  //display.setCursor(0,0);
  //display.println(String(tempIn) + " ºC " + String(tempOut) + " ºC " + String(pot) + " W");



}


/*
 * Transforma los valores ADC a ºC. Se asume que los dos
 * termistores son iguales (misma B).
 * Parámetros:
 * adc: valor obtenido con analogRead()
 * sr: resistencia en serie con el termistor para hacer el
 *     divisor de tensión.
 */
double adc2temp(int adc, int sr){
  //convert ADC value to resistance
  adc = 1023 / adc - 1;
  adc = sr / adc;

  double steinhart;
  steinhart = adc / THERMISTORNOMINAL;                  // (R/Ro)
  steinhart = log(steinhart);                           // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                            // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15);     // + (1/To)
  steinhart = 1.0 / steinhart;                          // Invert
  steinhart -= 273.15;                                  // convert to C

  return steinhart;
}
























/*
  // draw a single pixel
  display.drawPixel(10, 10, WHITE);
  // Show the display buffer on the hardware.
  // NOTE: You _must_ call display after making any drawing commands
  // to make them visible on the display hardware!
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw many lines
  testdrawline();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw rectangles
  testdrawrect();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw multiple rectangles
  testfillrect();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw mulitple circles
  testdrawcircle();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw a white circle, 10 pixel radius
  display.fillCircle(display.width()/2, display.height()/2, 10, WHITE);
  display.display();
  delay(2000);
  display.clearDisplay();

  testdrawroundrect();
  delay(2000);
  display.clearDisplay();

  testfillroundrect();
  delay(2000);
  display.clearDisplay();

  testdrawtriangle();
  delay(2000);
  display.clearDisplay();

  testfilltriangle();
  delay(2000);
  display.clearDisplay();

  // draw the first ~12 characters in the font
  testdrawchar();
  display.display();
  delay(2000);
  display.clearDisplay();

  // draw scrolling text
  testscrolltext();
  delay(2000);
  display.clearDisplay();

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Hello, world!");
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(3.141592);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print("0x"); display.println(0xDEADBEEF, HEX);
  display.display();
  delay(2000);
  display.clearDisplay();

  // miniature bitmap display
  display.drawBitmap(30, 16,  logo16_glcd_bmp, 16, 16, 1);
  display.display();
  delay(1);

  // invert the display
  display.invertDisplay(true);
  delay(1000);
  display.invertDisplay(false);
  delay(1000);
  display.clearDisplay();

  // draw a bitmap icon and 'animate' movement
  testdrawbitmap(logo16_glcd_bmp, LOGO16_GLCD_HEIGHT, LOGO16_GLCD_WIDTH);

void testdrawbitmap(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  uint8_t icons[NUMFLAKES][3];

  // initialize
  for (uint8_t f=0; f< NUMFLAKES; f++) {
    icons[f][XPOS] = random(display.width());
    icons[f][YPOS] = 0;
    icons[f][DELTAY] = random(5) + 1;

    Serial.print("x: ");
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(" y: ");
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(" dy: ");
    Serial.println(icons[f][DELTAY], DEC);
  }

  while (1) {
    // draw each icon
    for (uint8_t f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, WHITE);
    }
    display.display();
    delay(200);

    // then erase it + move it
    for (uint8_t f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, BLACK);
      // move it
      icons[f][YPOS] += icons[f][DELTAY];
      // if its gone, reinit
      if (icons[f][YPOS] > display.height()) {
        icons[f][XPOS] = random(display.width());
        icons[f][YPOS] = 0;
        icons[f][DELTAY] = random(5) + 1;
      }
    }
   }
}


void testdrawchar(void) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  for (uint8_t i=0; i < 168; i++) {
    if (i == '\n') continue;
    display.write(i);
    if ((i > 0) && (i % 21 == 0))
      display.println();
  }
  display.display();
  delay(1);
}

void testdrawcircle(void) {
  for (int16_t i=0; i<display.height(); i+=2) {
    display.drawCircle(display.width()/2, display.height()/2, i, WHITE);
    display.display();
    delay(1);
  }
}

void testfillrect(void) {
  uint8_t color = 1;
  for (int16_t i=0; i<display.height()/2; i+=3) {
    // alternate colors
    display.fillRect(i, i, display.width()-i*2, display.height()-i*2, color%2);
    display.display();
    delay(1);
    color++;
  }
}

void testdrawtriangle(void) {
  for (int16_t i=0; i<min(display.width(),display.height())/2; i+=5) {
    display.drawTriangle(display.width()/2, display.height()/2-i,
                     display.width()/2-i, display.height()/2+i,
                     display.width()/2+i, display.height()/2+i, WHITE);
    display.display();
    delay(1);
  }
}

void testfilltriangle(void) {
  uint8_t color = WHITE;
  for (int16_t i=min(display.width(),display.height())/2; i>0; i-=5) {
    display.fillTriangle(display.width()/2, display.height()/2-i,
                     display.width()/2-i, display.height()/2+i,
                     display.width()/2+i, display.height()/2+i, WHITE);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
    delay(1);
  }
}

void testdrawroundrect(void) {
  for (int16_t i=0; i<display.height()/2-2; i+=2) {
    display.drawRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, WHITE);
    display.display();
    delay(1);
  }
}

void testfillroundrect(void) {
  uint8_t color = WHITE;
  for (int16_t i=0; i<display.height()/2-2; i+=2) {
    display.fillRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, color);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
    delay(1);
  }
}

void testdrawrect(void) {
  for (int16_t i=0; i<display.height()/2; i+=2) {
    display.drawRect(i, i, display.width()-2*i, display.height()-2*i, WHITE);
    display.display();
    delay(1);
  }
}

void testdrawline() {
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(0, 0, display.width()-1, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, display.height()-1, i, 0, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=display.height()-1; i>=0; i-=4) {
    display.drawLine(0, display.height()-1, display.width()-1, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i=display.width()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, i, 0, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=display.height()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, 0, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();
  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, WHITE);
    display.display();
    delay(1);
  }
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(display.width()-1, 0, i, display.height()-1, WHITE);
    display.display();
    delay(1);
  }
  delay(250);
}

void testscrolltext(void) {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10,0);
  display.clearDisplay();
  display.println("scroll");
  display.display();
  delay(1);

  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
}
*/
