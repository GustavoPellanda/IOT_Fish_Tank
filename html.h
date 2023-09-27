#include <pgmspace.h>

const char PAGE_MAIN[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width">
        <title>Fish Tank Overview</title>
        <link rel="icon" href="data">
        <style>
            body {
                background-color: #f2f2f2;
                font-family: Arial, sans-serif;
                text-align: center;
            }

            h1 {
                font-size: 36px;
                color: #3498db;
                margin-top: 40px;
            }

            h2 {
                font-size: 24px;
                color: #444444;
                margin-bottom: 20px;
            }

            .container {
                display: grid;
                grid-template-columns: repeat(3, 1fr);
                grid-gap: 20px;
                margin-top: 40px;
                margin-bottom: 60px;
                margin-left: 10px;
                margin-right: 10px;
            }

            .bodytext {
                font-size: 24px;
                font-weight: light;
                border-radius: 5px;
                padding: 10px;
                background-color: #ffffff;
                color: #333333;
                display: flex;
                align-items: center;
                justify-content: space-between;
                box-shadow: 0px 2px 4px rgba(0, 0, 0, 0.1);
            }

            .bodytext::after {
                content: '';
                display: block;
                clear: both;
            }

            .bodytext:before {
                content: '';
                display: block;
                clear: both;
            }

            .bodytext > div {
                display: flex;
                align-items: center;
                margin-left: 10px;
            }

            .bodytext > div:first-child {
                flex-grow: 1;
            }

            .bodytext > div:last-child {
                margin-left: auto;
                flex-shrink: 1;
                margin-left: 5px;
                margin-right: 10px;
            }

            .unit {
                flex-shrink: 1;
                flex-basis: auto;
            }

            
            .checkbox {
                position: relative;
                cursor: pointer;
                padding-left: 35px;
                margin-bottom: 24px;
                display: inline-block;
                vertical-align: middle;
            }

            .checkbox input {
                position: absolute;
                opacity: 0;
                cursor: pointer;
            }

            .checkmark {
                position: absolute;
                top: 0;
                left: 0;
                height: 25px;
                width: 25px;
                background-color: #f2f2f2;
                border: 2px solid #3498db;
                border-radius: 5px;
            }

            .checkbox:hover input ~ .checkmark {
                background-color: #e6e6e6;
            }

            .checkbox input:checked ~ .checkmark {
                background-color: #3498db;
                border: 2px solid #3498db;
            }

            .checkmark:after {
                content: "";
                position: absolute;
                display: none;
            }

            .checkbox input:checked ~ .checkmark:after {
                display: block;
            }

            .checkbox .checkmark:after {
                left: 7px;
                top: 3px;
                width: 5px;
                height: 10px;
                border: solid #fff;
                border-width: 0 2px 2px 0;
                transform: rotate(45deg);
            }

            .last {
                display: flex;
                justify-content: center;
                align-items: center;
            }
        </style>
    </head>

    <body onload="fetchData()">
        <h1>Fish Tank Overview</h1>
        <h2>Monitoring</h2>
        <div class="container">
            <div class="bodytext">
                <div class="container-text">Temperature:</div>
                <div id="temp"></div>
                <div class="unit">°C</div>
            </div>
            <div class="bodytext">
                <div>Level:</div>
                <div id="dist"></div>
                <div class="unit">%</div>
            </div>
            <div class="bodytext">
                <div>Luminosity:</div>
                <div id="lum"></div>
                <div class="unit">%</div>
            </div>
        </div>
        <h2>Control</h2>
        <div class="container">
            <div class="bodytext">
                <div>Pump 1 control:</div>
                <div class="checkbox">
                    <input type="checkbox" id="ctrlpump1" onclick="controlPump(1)">
                    <label for="ctrlpump1" class="checkmark"></label>
                </div>
            </div>
            <div class="bodytext">
                <div>Pump 2 control:</div>
                <div class="checkbox">
                    <input type="checkbox" id="ctrlpump2" onclick="controlPump(2)">
                    <label for="ctrlpump2" class="checkmark"></label>
                </div>
            </div>
            <div class="bodytext">
                <div>Pump 3 control:</div>
                <div class="checkbox">
                    <input type="checkbox" id="ctrlpump3" onclick="controlPump(3)">
                    <label for="ctrlpump3" class="checkmark"></label>
                </div>
            </div>
        </div>
        <h2>Alerts</h2>
        <div class="container">
            <div class="bodytext">
                <div>Temperature alert:</div>
                <div id="alertTemp" class="alert"></div>
            </div>
            <div class="bodytext">
                <div>Distance alert:</div>
                <div id="alertDist" class="alert"></div>
            </div>
            <div class="bodytext">
                <div>Luminosity alert:</div>
                <div id="alertLum" class="alert"></div>
            </div>
        </div>
        <h2>Action Log</h2>
        <div class="last container">
            <div class="bodytext">
                <div>Action log:</div>
                <div id="log"></div>
            </div>
        </div>
        <script>
            function fetchData() {
                var xhttp = new XMLHttpRequest();
                xhttp.onreadystatechange = function() {
                    if (this.readyState === 4 && this.status === 200) {
                        // Parse the XML response
                        var xmlDoc = this.responseXML;
    
                        // Extract sensor data from the XML
                        var temperature = xmlDoc.getElementsByTagName("TEMP")[0].textContent;
                        var waterLevel = xmlDoc.getElementsByTagName("DIST")[0].textContent;
                        var luminosity = xmlDoc.getElementsByTagName("LUM")[0].textContent;
    
                        // Update the placeholders with the fetched data
                        document.getElementById("temp").textContent = temperature;
                        document.getElementById("dist").textContent = waterLevel;
                        document.getElementById("lum").textContent = luminosity;
                    }
                };
    
                // Send a GET request to the ESP32's XML endpoint
                xhttp.open("GET", "/xml", true);
                xhttp.send();
            }
    
            // Set an interval to periodically fetch and update data
            setInterval(fetchData, 5000);
    
            // Define variables to store pump states
            var pumpStates = {
                pump1: false,
                pump2: false,
                pump3: false
            };

            // Function to update pump state when checkbox is clicked
            function controlPump(pumpId) {
                var checkbox = document.getElementById("ctrlpump" + pumpId);
                pumpStates["pump" + pumpId] = checkbox.checked;

                // Send the updated pump state to the ESP32 immediately
                sendPumpControl(pumpId, checkbox.checked);
            }

            // Function to send pump control settings to ESP32
            function sendPumpControl(pumpId, state) {
                var jsonData = JSON.stringify({ pumpId: pumpId, state: state });
                var xhttp = new XMLHttpRequest();
                var endpointUrl = "/controlPump";
                xhttp.open("POST", endpointUrl, true);
                xhttp.setRequestHeader("Content-Type", "application/json;charset=UTF-8");
                xhttp.send(jsonData);
            }

        </script>
    </body>
    
</html>
)=====";
