<h1 align="center"> IOT Fish Tank Control and Monitoring </h1>

<p> The IOT Fish Tank Control and Monitoring project is a system designed to automate fish tank cleaning and maintenance. It utilizes an ESP32 microcontroller to manage three water pumps responsible for transferring water between three tanks: a breeding tank, a clean water tank, and a tank for dirty water.

A luminosity sensor is used to detect the clarity of the water at the bottom of the tank. If the water becomes too cloudy, the system automatically transfers it to the dirty water tank for cleaning, and replaces it with clean water from the clean water tank.

An ultrasonic sensor is used to monitor the water level in the tank, ensuring that the water remains at a consistent level, and preventing it from overflowing or becoming too low.

A submersible thermometer and heater are used for controlling the water temperature, ensuring that the fish are kept in a healthy and comfortable environment.</p>
<p></p>

<h2>Content</h2>
  <ul>
    <li><a href="#Used Technologies">Used Technologies</a></li>
    <li><a href="#Code Explanation">Code Explanation</a></li>
    <li><a href="#Usage">Usage</a></li>
    <li><a href="#Web Inerface">Web Inerface</a></li>
    <li><a href="#Building">Building</a></li>
  </ul>  

<h2>Used Technologies</h2>
<ul>
  <li>C/C++</li>
  <li>ESP32</li>
  <li>HTML & CSS</li>
  <li>Javascript</li>
  <li>Arduino.h</li>
  <li>OneWire.h</li>
  <li>DallasTemperature.h</li>
  <li>WiFi.h</li>
  <li>WebServer.h</li>
</ul>

<h2>Code Explanation</h2>
   <p>Fish_Tank_Ctrl is an Arduino code that reads temperature, luminosity, and distance sensors to control a water pump, a heater, and two solenoid valves. The code also creates a web server to display the sensor readings and control the system remotely.

The code starts by including the OneWire, DallasTemperature, WiFi, and WebServer libraries and defining the network credentials for connecting to an existing network or creating a new access point. Next, the sound speed, maximum and minimum temperature, distance, and luminosity thresholds constants are created, as well as the periods for reading each sensor.

The variables created are time counters for each sensor and the flags that control the state of the system. It also sets the pin modes for the sensors, the water pump, the heater, and the solenoid valves.

The setup function initializes the Serial Monitor, starts the temperature sensor, and connects to the network or creates an access point. It also sets up the web server and defines the handlers for the root and /xml routes.

The loop function reads the temperature sensor, the luminosity sensor, and the distance sensor at their respective periods. It then updates the flags that control the state of the system based on the sensor readings. Finally, it calls the PumpControl function, which updates the state of the system by turning on or off the water pump, the heater, and the solenoid valves.

The Temperature function uses the DallasTemperature library to read the temperature from a DS18B20 sensor. The function requests the temperature and returns it as a float value.

The waterPumpTemperature function checks if the temperature is within the allowed range and updates the flag for the water pump accordingly.

The Ultrasonic function uses the HC-SR04 ultrasonic sensor to measure the distance to the water level. The function calculates the distance and returns it as a float value.

The waterPumpUltrasonic function checks if the water level is within the allowed range and updates the flags for the solenoid valves accordingly.

The ldrValue function reads the value from an LDR sensor and returns it as an int value.

The waterPumpLDR function checks if the luminosity is within the allowed range and updates the flag for the solenoid valves accordingly.

The PumpControl function updates the state of the system based on the flags for the sensors and the solenoid valves. It turns on or off the water pump, the heater, and the solenoid valves as needed.

The SendWebsite function handles the root route and sends the HTML page to the client.

The sendXML function handles the /xml route and sends the XML data to the client.</p>

<h2>Usage</h2>
   <p>The .cpp and the .h files must be downloaded to the ESP32. The html.h is where the HTML code will be contained.</p>

<h2>Web Interface</h2>
  <p>This is the web interface, from which you can monitor and control the fish tanks:</p>
  <img src="https://user-images.githubusercontent.com/129123498/233806760-cb87e62e-f622-40d8-92cc-4343f308a9ed.png" alt="Gráfico sendo exibido na web">
  
<h2>Building</h2> 
 <p>🚧 22/04/2023 - At the moment, the functions that communicate between the Web Interface and the controller are still under development.</p>
