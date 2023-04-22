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

    <body onload="process()">
        <h1>Fish Tank Overview</h1>
        <h2>Monitoring</h2>
        <div class="container">
            <div class="bodytext">
                <div class="conatainer-text">Temperature:</div>
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
            <div class="bodytext">
                <div>Pump 01:</div>
                <div id="pump1"></div>
                <div class="unit"></div>
            </div>
            <div class="bodytext">
                <div>Pump 02:</div>
                <div id="pump2"></div>
                <div class="unit"></div>
            </div>
            <div class="bodytext">
                <div>Pump 03:</div>
                <div id="pump3"></div>
                <div class="unit"></div>
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
                <div id="alertTemp"></div>
            </div>
            <div class="bodytext">
                <div>Distance alert:</div>
                <div id="alertDist"></div>
            </div>
            <div class="bodytext">
                <div>Luminosity alert:</div>
                <div id="alertLum"></div>
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
            function process() {
                // TO DO: Code for processing data and updating UI
    
                // Mockup data
                var tempData = "25.5";
                var distData = "80";
                var lumData = "70";
                var pump1Data = "ON";
                var pump2Data = "OFF";
                var pump3Data = "ON";
                var alertTempData = "No alerts";
                var alertDistData = "No alerts";
                var alertLumData = "No alerts";
                var logData = "pump1 ON at 10:30";
    
                // Update UI
                document.getElementById("temp").innerHTML = tempData;
                document.getElementById("dist").innerHTML = distData;
                document.getElementById("lum").innerHTML = lumData;
                document.getElementById("pump1").innerHTML = pump1Data;
                document.getElementById("pump2").innerHTML = pump2Data;
                document.getElementById("pump3").innerHTML = pump3Data;
                document.getElementById("alertTemp").innerHTML = alertTempData;
                document.getElementById("alertDist").innerHTML = alertDistData;
                document.getElementById("alertLum").innerHTML = alertLumData;
                document.getElementById("log").innerHTML = logData;
            }
    
            function controlPump(pumpId) {
                // TO DO: Code for controlling pumps
    
                var pumpStatus = document.getElementById("ctrlpump" + pumpId).checked;
                var logMsg = "";
    
                if (pumpStatus) {
                    // Pump is turned on
                    logMsg = "pump" + pumpId + " ON";
                } else {
                    // Pump is turned off
                    logMsg = "pump" + pumpId + " OFF";
                }
    
                // Update log
                var logElement = document.getElementById("log");
                logElement.innerHTML += "<br>" + logMsg;
            }
        </script>
    </body>
    
</html>
)=====";