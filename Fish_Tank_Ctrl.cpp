#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>
#include "html.h"

// Internet and password
#define LOCAL_SSID "TEST"
#define LOCAL_PASS "12345678"

// Client will connect 
#define AP_SSID "TEST"
#define AP_PASS "12345678"

// Define sound speed in cm/uS
#define SOUND_SPEED 0.034

// Max/Min temperature to activate the water pump
#define TEMPERATURE_MAX 26.0
#define TEMPERATURE_MIN 25.0

// Max/Min height to activate the water pump
// Limits the water maximum water height to 100% and the minimum to 69%.
#define DISTANCE_MAX 7.0
#define DISTANCE_MED 5.5
#define DISTANCE_MIN 4.0

// Water pump activation through luminosity
// Water will flow from 52% to 79% luminosity
#define LUMINOSITY_THRESHOLD_MIN 1140
#define LUMINOSITY_THRESHOLD_MAX 1300

// Period of each sensor
#define PER_TEMPERATURE 3000
#define PER_LDR 3000
#define PER_DIST 500

#define USE_INTRANET

// Send information to the web server
char XML[2048];
char buf[32];

IPAddress Actual_IP;

IPAddress PageIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip;

// Time
int time_temperature = millis() + PER_TEMPERATURE;
int time_ldr = millis() + PER_LDR;
int time_dist = millis() + PER_DIST;

// GPIO where the DS18B20 is connected 
const int oneWireBus = 32;     

int counter_in = 0;
int counter_out = 0;
int counter_stop = 0;

// GPIO where the LDR is connected
const int LDR = 35;

// GPIO where the water pump is activated
// pump - fills
// pump2 - empties 
const int pump = 18;
const int pump2 = 19;

// GPIO where the heater is activated
const int heater = 21;

// GPIOS where the HC-SR04 is connected 
const int trigPin = 5;
const int echoPin = 33;

long duration;
float distanceCm;
float distance;
float temperature;
int luminosity;
float distancePercentage;
float luminosistyPercentage;

// flags
bool flag_temp_on = false;
bool flag_ldr_on = false;
bool flag_dist_pump1_on = false;
bool flag_dist_pump2_on = false;
bool flag_dist_isMoreImportant = true;
bool flag_all_off = true; // all pumps off
bool pump1_rele = false;
bool pump2_rele = false;
bool heater_rele = false;

// Setup an oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Passes our oneWire reference to Dallas temperature sensor 
DallasTemperature sensors(&oneWire);

// Creates web server object that listens on port 80
WebServer server(80); 

// Functions
float Temperature();
void waterPumpTemperature(float temperature);
float Ultrasonic();
void waterPumpUltrasonic(float distance);
int ldrValue();
void waterPumpLDR(int valueLDR);
void PumpControl();
void SendWebsite();
void sendXML();
float lumPorcentage(int luminosity);
float distPorcentage(float dist);


void setup() {

  // Starts the Serial Monitor
  Serial.begin(115200);
  // Starts the DS18B20 sensor
  sensors.begin();
  // Set GPIOs as output or input
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(pump, OUTPUT);
  pinMode(pump2, OUTPUT);
  pinMode(heater, OUTPUT);

  #ifdef USE_INTRANET
    WiFi.begin(LOCAL_SSID, LOCAL_PASS);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      delay(500);
      Serial.print(WiFi.waitForConnectResult());
      Serial.print(".");
      Serial.print(WiFi.status());
    }
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    Actual_IP = WiFi.localIP();
  #endif

  #ifndef USE_INTRANET
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(100);
    WiFi.softAPConfig(PageIP, gateway, subnet);
    delay(100);
    Actual_IP = WiFi.softAPIP();
    Serial.print("IP address: "); Serial.println(Actual_IP);
  #endif

  server.on("/", SendWebsite);
  server.on("/xml", sendXML);

  server.begin();
}

void loop() {
  // Temperature sensor
  if(millis() >= time_temperature) {
    time_temperature = millis() + PER_TEMPERATURE;
    temperature = Temperature();
    waterPumpTemperature(temperature);
  }

  // Luminosity Sensor 
  if(millis() >= time_ldr) {
    time_ldr = millis() + PER_LDR;
    luminosity = ldrValue();
    luminosistyPercentage = lumPorcentage(luminosity);
    waterPumpLDR(luminosity);
  }
  
  // Level Sensor
  if(millis() >= time_dist) {
    time_dist = millis() + PER_DIST;
    distance = Ultrasonic();
    distancePercentage = distPorcentage(distance);
    waterPumpUltrasonic(distance);
  }

  PumpControl();
  server.handleClient();
}

float Temperature() {
  sensors.requestTemperatures(); 
  return sensors.getTempCByIndex(0);
}

float Ultrasonic() {
   // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED/2;

  return distanceCm;
}

// Function sensor LDR
int ldrValue() {
  // reads the value on input (between 0 and 4095)
  return analogRead(LDR);
}

// Function to activate water pump with luminosity
void waterPumpLDR(int valueLDR) {
  if(valueLDR < LUMINOSITY_THRESHOLD_MIN) {
    flag_ldr_on = true;
  }
  else if(valueLDR > LUMINOSITY_THRESHOLD_MAX) {
    flag_ldr_on = false;
  }
}

// Function to activate water pump with temperature
void waterPumpTemperature(float temperature) {
  if(temperature > TEMPERATURE_MAX) {
    flag_temp_on = true;
    heater_rele = false;
  }
  else if(temperature < TEMPERATURE_MIN) {
    digitalWrite(heater, LOW);
    heater_rele = true;
  }
  else {
    digitalWrite(heater, HIGH);
    flag_temp_on = false;
    heater_rele = false;
  }
}

// Function to activate water pump with level
void waterPumpUltrasonic(float distance) {
  if(distance > DISTANCE_MAX) {
    counter_in++;
    counter_out = 0;
    counter_stop = 0;
    digitalWrite(pump, LOW);
    digitalWrite(pump2, HIGH);
  }
  else if(distance < DISTANCE_MIN ) {
    counter_out++;
    counter_in = 0;
    counter_stop = 0;
  }
  else if(distance > DISTANCE_MIN && distance < DISTANCE_MAX) {
    counter_stop++;
    counter_in = 0;
    counter_out = 0;
  }
  if(counter_in >= 3) {
    flag_dist_isMoreImportant = true;
    flag_dist_pump1_on = true;
    flag_dist_pump2_on = false;
    counter_in = 0;
  }
  else if(counter_out >= 3) {
    flag_dist_isMoreImportant = true;
    flag_dist_pump1_on = false;
    flag_dist_pump2_on = true;
    counter_out = 0;
  }
  else if(counter_stop >= 3) {
    flag_dist_isMoreImportant = false;
    flag_dist_pump1_on = false;
    flag_dist_pump2_on = false;
    counter_stop = 0;
  }
}

void PumpControl() {
  if(flag_dist_isMoreImportant) {
    if(flag_dist_pump1_on) {
      digitalWrite(pump, LOW);
      digitalWrite(pump2, HIGH);
      pump1_rele = true;
      pump2_rele = false;
    }
    else if(flag_dist_pump2_on){
      digitalWrite(pump, HIGH);
      digitalWrite(pump2, LOW);
      pump2_rele = true;
      pump1_rele = false;
    }
  }
  else if(flag_temp_on){
	digitalWrite(pump, LOW);
    digitalWrite(pump2, HIGH);
    pump1_rele = true;
    pump2_rele = false;
  }
  else if(flag_ldr_on && flag_all_off){
    digitalWrite(pump, HIGH);
    digitalWrite(pump2, LOW);
    pump1_rele = false;
    pump2_rele = true;
  }
  else if(!flag_ldr_on) { //shuts the pumps off when the water is clear
    digitalWrite(pump, HIGH);
    digitalWrite(pump2, HIGH);
    pump1_rele = false;
    pump2_rele = false;
	flag_all_off = true;
  }
}

float lumPorcentage(int luminosity) {
  return (luminosity * 100.) / 1900;
}

float distPorcentage(float dist) {
  return ((13.8 - dist) * 100) / 9.8;
}

void SendWebsite() {
  server.send(200, "text/html", PAGE_MAIN);
}

void sendXML() {
  strcpy(XML, "<?xml version = '1.0'?>\n<Data>\n");

  //Sends temperature
  sprintf(buf, "<TEMP>%.2f</TEMP>\n", temperature);
  strcat(XML, buf);

  //Sends distance
  sprintf(buf, "<DIST>%.2f</DIST>\n", distancePercentage);
  strcat(XML, buf);

  //Sends luminosity
  sprintf(buf, "<LUM>%.2f</LUM>\n", luminosistyPercentage);
  strcat(XML, buf);

  sprintf(buf, "<PUMP1>%d</PUMP1>\n", pump1_rele);
  strcat(XML, buf);

  sprintf(buf, "<PUMP2>%d</PUMP2>\n", pump2_rele);
  strcat(XML, buf);

  sprintf(buf, "<HEATER>%d</HEATER>\n", heater_rele);
  strcat(XML, buf);

  strcat(XML, "</Data>\n");

  server.send(200, "text/xml", XML);
}