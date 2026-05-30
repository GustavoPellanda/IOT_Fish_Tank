# IoT Fish Tank Control and Monitoring

## Overview

This project implements an embedded monitoring and control system for automated fish tank maintenance using an ESP32 microcontroller. The system continuously monitors water conditions and automatically controls pumps and heating elements to maintain a stable environment for the fish.

The firmware integrates environmental sensing, actuator control, and a lightweight web interface into a single embedded application. Sensor data is collected periodically and processed by a deterministic control loop responsible for regulating water level, temperature, and water quality.

The system was designed to remain lightweight and easy to deploy while still following structured embedded software design practices.

---

## Features

- Real-time monitoring through a web dashboard
- Automatic water level regulation
- Automatic temperature control
- Water replacement based on luminosity/turbidity conditions
- Remote actuator control through HTTP requests
- Wi-Fi station mode or ESP32 access point mode
- Lightweight single-file embedded firmware architecture

---

## System Architecture

The project controls water circulation between three tanks:

1. Main fish tank
2. Clean water tank
3. Dirty water tank

The ESP32 reads environmental data from multiple sensors and decides when to activate the actuators according to predefined thresholds and priorities.

### Sensor Inputs

| Sensor   | Function                              |
|----------|---------------------------------------|
| DS18B20  | Water temperature measurement         |
| HC-SR04  | Water level measurement               |
| LDR      | Water luminosity / turbidity estimation |

### Controlled Devices

| Device              | Function                          |
|---------------------|-----------------------------------|
| Water Pump (Fill)   | Adds clean water to the tank      |
| Water Pump (Drain)  | Removes dirty water               |
| Heater              | Maintains water temperature       |

---

## Control Logic

The firmware operates using periodic non-blocking tasks. Each sensor is sampled at a predefined interval and the system updates its internal state accordingly.

The actuator control logic follows a priority hierarchy:

1. Water level protection
2. Temperature regulation
3. Luminosity-based cleaning

This prevents conflicting actuator states and ensures deterministic system behavior.

### Water Level Regulation

The ultrasonic sensor continuously measures the distance between the sensor and the water surface. If the level becomes too high or too low, the system automatically activates the appropriate pump.

### Temperature Regulation

The DS18B20 temperature sensor monitors the water temperature. If the temperature falls below the configured threshold, the heater is activated. If the temperature exceeds the upper threshold, the system can trigger water circulation for cooling.

### Water Quality Monitoring

An LDR sensor is used as a simple approximation of water clarity. When luminosity values indicate excessive turbidity, the system drains dirty water and replaces it with clean water.

---

## Web Interface

The ESP32 hosts a lightweight embedded web server that provides:

- Real-time telemetry visualization
- Sensor monitoring
- Pump state visualization
- Remote actuator control
- System event feedback

The interface is implemented using embedded HTML, CSS, and JavaScript stored directly in program memory.

---

## Technologies Used

### Hardware

- ESP32
- DS18B20 temperature sensor
- HC-SR04 ultrasonic sensor
- LDR luminosity sensor
- Relay modules
- Water pumps
- Aquarium heater

### Libraries

- `Arduino.h`
- `WiFi.h`
- `WebServer.h`
- `OneWire.h`
- `DallasTemperature.h`
- `ArduinoJson.h`

---

## Usage

### 1. Configure Wi-Fi

Edit the network credentials in the source code:

```cpp
#define LOCAL_SSID "YOUR_WIFI"
#define LOCAL_PASS "YOUR_PASSWORD"
```

Alternatively, the ESP32 can operate in Access Point mode.

### 2. Upload Firmware

Compile and upload the firmware to the ESP32.


### 3. Open the Web Interface

After booting, the ESP32 prints its IP address to the Serial Monitor:

```
IP: 192.168.x.x
```

Open the address in a browser to access the monitoring dashboard.
