#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

#include "html.h"

// Network configuration:
#define USE_INTRANET
static constexpr const char* LOCAL_SSID = "TEST";
static constexpr const char* LOCAL_PASS = "12345678";
static constexpr const char* AP_SSID = "TEST";
static constexpr const char* AP_PASS = "12345678";

// Hardware pins:
namespace Pin
{
  constexpr uint8_t ONE_WIRE = 32;
  constexpr uint8_t LDR = 35;
  constexpr uint8_t PUMP_IN = 18;
  constexpr uint8_t PUMP_OUT = 19;
  constexpr uint8_t HEATER = 21;
  constexpr uint8_t TRIG = 5;
  constexpr uint8_t ECHO = 33;
}

// System thresholds and constants:
static constexpr float SOUND_SPEED_CM_US = 0.034f;

static constexpr float TEMP_MAX = 26.0f;
static constexpr float TEMP_MIN = 25.0f;

static constexpr float DIST_MAX = 7.0f;
static constexpr float DIST_MIN = 4.0f;

// Distance-to-percent reference points (sensor distance at empty and full tank):
static constexpr float DIST_SENSOR_EMPTY = 13.8f;
static constexpr float DIST_SENSOR_RANGE = 9.8f;

static constexpr int LDR_MIN = 1140;
static constexpr int LDR_MAX = 1300;

// LDR ADC full-scale reference for percent conversion:
static constexpr float LDR_FULL_SCALE = 1900.0f;

// Task periods (in milliseconds):
static constexpr uint32_t TEMP_PERIOD_MS = 3000;
static constexpr uint32_t LDR_PERIOD_MS  = 3000;
static constexpr uint32_t DIST_PERIOD_MS = 500;

// Pump actuator states:
enum class PumpMode {
  OFF,
  FILL,
  DRAIN
};

// Variables mesured by sensors:
struct SensorData {
  float temperatureC = 0.0f;
  float distanceCm = 0.0f;
  float distancePercent = 0.0f;
  int luminosityRaw = 0;
  float luminosityPercent = 0.0f;
  bool distanceValid = false;
};

// System state variables:
struct SystemState {
  PumpMode pumpMode = PumpMode::OFF;
  bool heaterEnabled = false;
  bool tempDemand = false;
  bool ldrDemand = false;
  bool levelHigh = false;
  bool levelLow = false;
  bool manualOverride = false;
};

// Global objects:
OneWire oneWire(Pin::ONE_WIRE);
DallasTemperature tempSensor(&oneWire);
WebServer server(80);
SensorData sensors;
SystemState state;

// Timestamps for periodic tasks:
uint32_t nextTempRead = 0;
uint32_t nextLdrRead  = 0;
uint32_t nextDistRead = 0;

// Helper function to check if a periodic task should run based on its timestamp and period:
static bool periodicTask(uint32_t& timestamp, uint32_t period) {
  const uint32_t now = millis();

  if ((int32_t)(now - timestamp) >= 0) {
    timestamp = now + period;
    return true;
  }
  return false;
}

// Sets the pump mode and updates the corresponding output pins:
static void setPumpMode(PumpMode mode) {
  state.pumpMode = mode;

  switch (mode) {
    case PumpMode::OFF:
      digitalWrite(Pin::PUMP_IN, HIGH);
      digitalWrite(Pin::PUMP_OUT, HIGH);
      break;

    case PumpMode::FILL:
      digitalWrite(Pin::PUMP_IN, LOW);
      digitalWrite(Pin::PUMP_OUT, HIGH);
      break;

    case PumpMode::DRAIN:
      digitalWrite(Pin::PUMP_IN, HIGH);
      digitalWrite(Pin::PUMP_OUT, LOW);
      break;
  }
}

// Sets the heater state and updates the corresponding output pin:
static void setHeater(bool enabled) {
  state.heaterEnabled = enabled;
  digitalWrite(Pin::HEATER, enabled ? LOW : HIGH);
}

// Sensor reading functions:
static float readTemperature() {
  tempSensor.requestTemperatures();
  return tempSensor.getTempCByIndex(0);
}

static float readDistanceCm() {
  digitalWrite(Pin::TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(Pin::TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(Pin::TRIG, LOW);
  const long duration = pulseIn(Pin::ECHO, HIGH, 30000);

  // Returns -1 as a sentinel value if the sensor timed out:
  if (duration == 0) return -1.0f;

  return (duration * SOUND_SPEED_CM_US) * 0.5f;
}

static int readLuminosity() {
  return analogRead(Pin::LDR);
}

// Conversion functions:
static float distanceToPercent(float distanceCm) {
  return ((DIST_SENSOR_EMPTY - distanceCm) * 100.0f) / DIST_SENSOR_RANGE;
}

static float luminosityToPercent(int raw) {
  return (raw * 100.0f) / LDR_FULL_SCALE;
}

// Updates tempDemand based on temperature sensor reading and thresholds:
static void updateTemperatureLogic() {
  if (sensors.temperatureC > TEMP_MAX) {
    state.tempDemand = true;
    setHeater(false);
  }
  else if (sensors.temperatureC < TEMP_MIN) {
    state.tempDemand = false;
    setHeater(true);
  }
  else {
    state.tempDemand = false;
    setHeater(false);
  }
}

// Updates ldrDemand based on luminosity sensor reading and thresholds:
static void updateLuminosityLogic() {
  if (sensors.luminosityRaw < LDR_MIN) state.ldrDemand = true;
  else if (sensors.luminosityRaw > LDR_MAX) state.ldrDemand = false;
}

// Updates levelHigh and levelLow based on distance sensor reading and thresholds:
static void updateLevelLogic() {
  // Skips level update if the distance reading is invalid (sensor timeout):
  if (!sensors.distanceValid) return;

  state.levelHigh = sensors.distanceCm > DIST_MAX;
  state.levelLow = sensors.distanceCm < DIST_MIN;
}

// Updates pump mode based on current system state and priorities:
static void updateActuators() {
  /*
    Priority:
    1. Water level protection
    2. Temperature control
    3. Luminosity control
  */

  // Skips automation if a manual override is active:
  if (state.manualOverride) return;

  if (state.levelHigh){
    setPumpMode(PumpMode::DRAIN);
    return;
  }
  if (state.levelLow){
    setPumpMode(PumpMode::FILL);
    return;
  }
  if (state.tempDemand){
    setPumpMode(PumpMode::DRAIN);
    return;
  }
  if (state.ldrDemand){
    setPumpMode(PumpMode::FILL);
    return;
  }
  setPumpMode(PumpMode::OFF);
}

// HTTP request handlers:
static void handleRoot() {
  server.send(200, "text/html", PAGE_MAIN);
}

static void handleXml() {
  char xml[512];

  snprintf(
    xml,
    sizeof(xml),
    "<?xml version='1.0'?>"
    "<Data>"
      "<TEMP>%.2f</TEMP>"
      "<DIST>%.2f</DIST>"
      "<LUM>%.2f</LUM>"
      "<PUMP>%d</PUMP>"
      "<HEATER>%d</HEATER>"
    "</Data>",

    sensors.temperatureC,
    sensors.distancePercent,
    sensors.luminosityPercent,
    static_cast<int>(state.pumpMode),
    state.heaterEnabled
  );

  server.send(200, "text/xml", xml);
}

// Handles POST requests to control the pump:
static void handlePumpControl() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }

  StaticJsonDocument<256> doc;
  const auto error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  const int pumpId = doc["pumpId"];
  const bool enabled = doc["state"];

  // Activates manual override so automation does not immediately cancel the command:
  state.manualOverride = enabled;

  // Sets the pump mode based on the Id and state recieved in the request:
  if (pumpId == 1) setPumpMode(enabled ? PumpMode::FILL : PumpMode::OFF);
  else if (pumpId == 2) setPumpMode(enabled ? PumpMode::DRAIN : PumpMode::OFF);
  
  else {
    server.send(400, "text/plain", "Invalid pump");
    return;
  }
  
  server.send(200, "text/plain", "OK");
}

// Initializes WiFi connection:
static void initializeWiFi() {
#ifdef USE_INTRANET

  WiFi.begin(LOCAL_SSID, LOCAL_PASS);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
    Serial.println("WiFi connection failed");
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

#else

  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

#endif
}

void setup() {
  Serial.begin(115200);

  pinMode(Pin::TRIG, OUTPUT);
  pinMode(Pin::ECHO, INPUT);
  pinMode(Pin::PUMP_IN, OUTPUT);
  pinMode(Pin::PUMP_OUT, OUTPUT);
  pinMode(Pin::HEATER, OUTPUT);

  setPumpMode(PumpMode::OFF);
  setHeater(false);

  tempSensor.begin();
  initializeWiFi();

  server.on("/", handleRoot);
  server.on("/xml", handleXml);
  server.on("/controlPump", HTTP_POST, handlePumpControl);
  server.begin();

  nextTempRead = millis();
  nextLdrRead  = millis();
  nextDistRead = millis();

  Serial.println("System initialized");
}

void loop() {

  // Periodically reads sensors and updates system state:
  if (periodicTask(nextTempRead, TEMP_PERIOD_MS)) {
    sensors.temperatureC = readTemperature();
    updateTemperatureLogic();
  }

  // Reads luminosity sensor and updates ldrDemand every LDR_PERIOD_MS:
  if (periodicTask(nextLdrRead, LDR_PERIOD_MS)) {
    sensors.luminosityRaw = readLuminosity();
    sensors.luminosityPercent = luminosityToPercent(sensors.luminosityRaw);
    updateLuminosityLogic();
  }

  // Reads distance sensor and updates levelHigh/levelLow every DIST_PERIOD_MS:
  if (periodicTask(nextDistRead, DIST_PERIOD_MS)) {
    const float raw = readDistanceCm();
    sensors.distanceValid = (raw > 0.0f);

    if (sensors.distanceValid) {
      sensors.distanceCm = raw;
      sensors.distancePercent = distanceToPercent(raw);
    }

    updateLevelLogic();
  }

  updateActuators();
  server.handleClient();
}