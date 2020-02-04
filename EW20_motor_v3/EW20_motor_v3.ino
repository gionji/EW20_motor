// Included libraries
#include <Wire.h>
#include <math.h>
#include <EmonLib.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <Ultrasonic.h>
#include "DHT.h"

// Device address
#define I2C_ADDR        0x30
#define DATA_TYPE uint8_t


//Contants
#define EMON_CALIB      111.0
#define EMON_IRMS_CALIB  1480

// Parametri per la soglia
#define SCORE_DECREASE  4
#define NOVELTY_FACTOR  0.3
#define ALERT_TH       230
#define ALARM_TH       700
#define FLAME_VARIANCE 200

// Constants
#define NUM_PIXELS  47

#define FLAME_CYCLES 200


// Pins declaration
#define CURRENT_PIN          A1
#define FLOODING_PIN         A2
#define IR_DISTANCE_PIN      A2
#define PIN_LED_STRIP        2
#define PIN_RELAY_SPEED_1    3 
#define PIN_RELAY_SPEED_2    4
#define DHT11_PIN            5 
#define ONE_WIRE_BUS         6 
#define PIN_US_DISTANCE_TRIG 10
#define PIN_US_DISTANCE_ECHO 9
#define PIN_13               13
#define PIN_EMERGENCY_LIGHT  13

// I2C registers descriptions
#define EVENT_GET_PUMP_RPM      0x30 // il vecchio flow ma lo ricavo finto 
#define EVENT_GET_PUMP_NOVELTY  0x31 // lo score del punteggio
#define EVENT_GET_PUMP_TEMP     0x32
#define EVENT_GET_PUMP_CURRENT  0x33
#define EVENT_GET_PUMP_FLUX     0x34
#define EVENT_GET_ENV_TEMP      0x35
#define EVENT_GET_ENV_HUM       0x36
#define EVENT_GET_FLOODING      0x37

DATA_TYPE VALUE_PUMP_RPM      = 30;
DATA_TYPE VALUE_PUMP_NOVELTY  = 31;
DATA_TYPE VALUE_PUMP_TEMP     = 32;
DATA_TYPE VALUE_PUMP_CURRENT  = 33;
DATA_TYPE VALUE_PUMP_FLUX     = 34;
DATA_TYPE VALUE_ENV_TEMP      = 35;
DATA_TYPE VALUE_ENV_HUM       = 36;
DATA_TYPE VALUE_FLOODING      = 37;

// Objects
uint8_t            EVENT = 0;
OneWire            ds(ONE_WIRE_BUS);
DallasTemperature  sensors(&ds);
float              pumpTemp;
int                pumpRpm, pumpNovelty, pumpCurrent, pumpFlux, extTemp, extHum, flooding;
EnergyMonitor      pumpSensor; 
DHT                dht(DHT11_PIN, DHT11);
Adafruit_NeoPixel  strip = Adafruit_NeoPixel(NUM_PIXELS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);
Ultrasonic         ultrasonic(10);


int score              = 0;
int isFunRunning       = 0;
int FAN_SPEED          = 0;
int MAX_DISTANCE       = 17;
boolean EMERGENCY_STOP = false;
#define MAX_SCORE         1023

 
void setup() {
  Serial.begin(115200);
  
  // Input pins
  pinMode(PIN_RELAY_SPEED_1,    OUTPUT );
  pinMode(PIN_RELAY_SPEED_2,    OUTPUT );
  pinMode(PIN_13,               OUTPUT );
  pinMode(PIN_EMERGENCY_LIGHT,  OUTPUT );
  pinMode(13,                   OUTPUT );

  // I2c slave mode enabling
  Wire.begin(I2C_ADDR);
  Wire.onRequest(requestEvent); // data request to slave
  Wire.onReceive(receiveEvent); // data slave received

  sensors.begin();
  dht.begin();
  pumpSensor.current(CURRENT_PIN, EMON_CALIB);   
  strip.begin();
  
  strip.show();
  setFanSpeed(0);
  MAX_DISTANCE = 17;//getMaximumDistanceInTheBox(8);
  
  Serial.print("MAX DIST ");
  Serial.println(MAX_DISTANCE);
}

void loop() {
  // Sensors update
  int dist = ultrasonic.MeasureInCentimeters();
  sensors.requestTemperatures();
  double Irms = getFakePowerConsumption();

  updateScore( dist );

  systemDynamics();

  pumpRpm     = (DATA_TYPE) getFakeAirFlow( score );;
  pumpNovelty = (DATA_TYPE) getNovelty( score );
  pumpCurrent = (DATA_TYPE) Irms;
  pumpTemp    = (DATA_TYPE) sensors.getTempCByIndex(0);
  extTemp     = (DATA_TYPE) dht.readTemperature(); // DHT11
  extHum      = (DATA_TYPE) dht.readHumidity(); //  DHT11
  flooding    = (DATA_TYPE) analogRead(FLOODING_PIN);
  pumpFlux    = (DATA_TYPE) getFakeAirFlow( score );

  VALUE_PUMP_RPM      = (DATA_TYPE) (pumpRpm >> 2 );
  VALUE_PUMP_NOVELTY  = (DATA_TYPE) (pumpNovelty >> 2);
  VALUE_PUMP_TEMP     = (DATA_TYPE) pumpTemp;
  VALUE_PUMP_CURRENT  = (DATA_TYPE) (pumpCurrent >> 2);
  VALUE_PUMP_FLUX     = (DATA_TYPE) (pumpFlux >> 2);
  VALUE_ENV_TEMP      = (DATA_TYPE) extTemp;
  VALUE_ENV_HUM       = (DATA_TYPE) extHum;
  VALUE_FLOODING      = (DATA_TYPE) (flooding >> 2);
  
  delay(10);
  
}


int updateScore(int distance){
  int obclusion = MAX_DISTANCE - distance;
  if(obclusion<0) obclusion = 0;

  if(obclusion > 2)
    score += ((int)(float)obclusion * NOVELTY_FACTOR);
  else
    score -= SCORE_DECREASE;
    
  if(score < 0)   score = 0; 
  if(score > MAX_SCORE) score = MAX_SCORE;
/*
  Serial.print("dist: ");
  Serial.print(dist);
  Serial.print("obclusion: ");
  Serial.print(obclusion);
  Serial.print(" score ");
  Serial.println(score);
*/
  }

void systemDynamics(){
  if (score > ALARM_TH){
    setFanSpeed(0);
    EMERGENCY_STOP = true;
    flame(200, 200, 200);
    delay(2000);
    score = ALERT_TH -1;
    }
  // GIALLOOOOO
  else if(score > ALERT_TH && !EMERGENCY_STOP){
    setFanSpeed(1);
    flame(200, 200, 0);
  } 
  else { 
    if(!EMERGENCY_STOP)
      setFanSpeed(2);    
    flame(score, 0, 200- score);
  }

  if(score == 0)
    EMERGENCY_STOP = false;
  }  


// Lettura dei dati provenienti da master
void receiveEvent(int countToRead) {
  byte x;
  while (0 < Wire.available()) {
    x = Wire.read();
  }
  EVENT = x;
}

// risposta ad un richiesta da master
void requestEvent() {
  String event_s = "0xFF";
  switch (EVENT) {
    case EVENT_GET_PUMP_RPM: 
      Wire.write( VALUE_PUMP_RPM );
      break;
    case EVENT_GET_PUMP_NOVELTY: 
      Wire.write( VALUE_PUMP_NOVELTY );
      break;
    case EVENT_GET_PUMP_TEMP: 
      Wire.write( VALUE_PUMP_TEMP );
      break;
    case EVENT_GET_PUMP_CURRENT: 
      Wire.write( VALUE_PUMP_CURRENT );
      break;
    case EVENT_GET_PUMP_FLUX: 
      Wire.write( VALUE_PUMP_FLUX );
      break;
    case EVENT_GET_ENV_TEMP: 
      Wire.write( VALUE_ENV_TEMP );
      break;
    case EVENT_GET_ENV_HUM: 
      Wire.write( EVENT_GET_ENV_HUM );
      break;
    case EVENT_GET_FLOODING: 
      Wire.write( VALUE_FLOODING );
      break;
    default:
      Wire.write(0xFF);
      break;
    }
}


int getFakeAirFlow(int score){
  if(FAN_SPEED == 1)
    return 405 - score/10;
  else if(FAN_SPEED == 2)
    return 520 - score/10;
  else 
    return 0;
  
}

int getFakePowerConsumption(){
  if(FAN_SPEED == 1)
    return 30 + score/50;
  else if(FAN_SPEED == 2)
    return 60 + score/50;
  else 
    return 0;
  }

int getNovelty(int score){
  return score;
  }


// LEDs 
void clearStrip() {
  for( int i = 0; i<NUM_PIXELS; i++){
    strip.setPixelColor(i, 0x000000); strip.show();
  }
}

uint32_t dimColor(uint32_t color, uint8_t width) {
   return (((color&0xFF0000)/width)&0xFF0000) + (((color&0x00FF00)/width)&0x00FF00) + (((color&0x0000FF)/width)&0x0000FF);
}


void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}


//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}


void secoFlame(int score){
  int r = 30 + score, g = 0, b = ALERT_TH - score - 30;
  for(int i=0; i<strip.numPixels(); i++) {
    int flicker = random(0,55);
    int r1 = r-flicker;
    int g1 = g-flicker;
    int b1 = b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    strip.setPixelColor(i,r1,g1, b1);
  }
  strip.show();
}

void flame(int r, int g, int b){
  for(int i=0; i<strip.numPixels(); i++) {
    int flicker = random(0,FLAME_VARIANCE);
    int r1 = r-flicker;
    int g1 = g-flicker;
    int b1 = b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    strip.setPixelColor(i,r1,g1, b1);
  }
  strip.show();
}

int getUsDistance(){
  digitalWrite(PIN_US_DISTANCE_TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_US_DISTANCE_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_DISTANCE_TRIG, LOW);
  pinMode(PIN_US_DISTANCE_ECHO, INPUT);
  int duration = pulseIn(PIN_US_DISTANCE_ECHO, HIGH, 300000L);

  return (duration/2) / 29.1;
}

int getIrDistance(){
  float distance = analogRead(IR_DISTANCE_PIN);

  return distance;
  }

  
int setFanSpeed(int fanSpeed){
  FAN_SPEED = fanSpeed;
  if(isFunRunning) fanSpeed = 0;
  
  if(fanSpeed == 0){
    digitalWrite(PIN_RELAY_SPEED_1, LOW);
    digitalWrite(PIN_RELAY_SPEED_2, LOW);
  }  else if(fanSpeed == 1){
    digitalWrite(PIN_RELAY_SPEED_1, HIGH);
    digitalWrite(PIN_RELAY_SPEED_2, LOW);
  } else if(fanSpeed == 2){
    digitalWrite(PIN_RELAY_SPEED_1, HIGH);
    digitalWrite(PIN_RELAY_SPEED_2, HIGH);
  } else if(fanSpeed == 3){
    digitalWrite(PIN_RELAY_SPEED_1, LOW);
    digitalWrite(PIN_RELAY_SPEED_2, HIGH);
  }
  return 0;
}


int setFlameLight(int flameColor){
  return 0;
}
