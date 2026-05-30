#include <pgmspace.h>

const char PAGE_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>Aquarium Controller</title>

<style>

:root
{
    --bg: #101418;
    --card: #1b222b;
    --card-border: #2b3542;

    --text: #e8eef5;
    --text-secondary: #9aa7b5;

    --accent: #4ea1ff;
    --good: #35c46b;
    --warn: #ffb020;
    --danger: #ff5d5d;

    --shadow:
        0 4px 12px rgba(0,0,0,0.25);
}

*
{
    box-sizing: border-box;
}

body
{
    margin: 0;
    padding: 24px;

    background: var(--bg);
    color: var(--text);

    font-family:
        Inter,
        system-ui,
        sans-serif;
}

.container
{
    max-width: 1200px;
    margin: 0 auto;
}

header
{
    margin-bottom: 32px;
}

h1
{
    margin: 0;
    font-size: 2rem;
    font-weight: 700;
}

.subtitle
{
    margin-top: 8px;
    color: var(--text-secondary);
}

.section
{
    margin-bottom: 32px;
}

.section-title
{
    margin-bottom: 16px;

    font-size: 0.95rem;
    font-weight: 600;
    letter-spacing: 0.08em;
    text-transform: uppercase;

    color: var(--text-secondary);
}

.grid
{
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
    gap: 16px;
}

.card
{
    background: var(--card);
    border: 1px solid var(--card-border);

    border-radius: 16px;

    padding: 20px;

    box-shadow: var(--shadow);
}

.metric-label
{
    font-size: 0.9rem;
    color: var(--text-secondary);
}

.metric-value
{
    margin-top: 12px;

    font-size: 2rem;
    font-weight: 700;
}

.metric-unit
{
    font-size: 1rem;
    color: var(--text-secondary);
}

.status
{
    display: inline-flex;
    align-items: center;
    gap: 8px;

    margin-top: 12px;

    font-size: 0.9rem;
}

.status-dot
{
    width: 10px;
    height: 10px;

    border-radius: 50%;
}

.status-good
{
    background: var(--good);
}

.status-warn
{
    background: var(--warn);
}

.status-danger
{
    background: var(--danger);
}

.control-row
{
    display: flex;
    justify-content: space-between;
    align-items: center;

    padding: 14px 0;

    border-bottom: 1px solid rgba(255,255,255,0.06);
}

.control-row:last-child
{
    border-bottom: none;
}

.switch
{
    position: relative;

    width: 54px;
    height: 28px;
}

.switch input
{
    opacity: 0;
    width: 0;
    height: 0;
}

.slider
{
    position: absolute;
    inset: 0;

    cursor: pointer;

    background: #4b5563;

    border-radius: 999px;

    transition: 0.2s;
}

.slider::before
{
    content: "";

    position: absolute;

    width: 22px;
    height: 22px;

    left: 3px;
    top: 3px;

    border-radius: 50%;

    background: white;

    transition: 0.2s;
}

input:checked + .slider
{
    background: var(--accent);
}

input:checked + .slider::before
{
    transform: translateX(26px);
}

.log
{
    font-family: monospace;

    font-size: 0.9rem;

    color: #c9d4df;

    max-height: 220px;
    overflow-y: auto;

    line-height: 1.5;
}

.footer
{
    margin-top: 48px;

    text-align: center;
    color: var(--text-secondary);

    font-size: 0.85rem;
}

@media (max-width: 600px)
{
    body
    {
        padding: 16px;
    }

    .metric-value
    {
        font-size: 1.6rem;
    }
}

</style>
</head>

<body>

<div class="container">

    <header>
        <h1>Aquarium Controller</h1>
        <div class="subtitle">
            ESP32 environmental monitoring and actuator control
        </div>
    </header>

    <section class="section">
        <div class="section-title">
            Telemetry
        </div>

        <div class="grid">

            <div class="card">
                <div class="metric-label">
                    Water Temperature
                </div>

                <div class="metric-value">
                    <span id="temp">--</span>
                    <span class="metric-unit">°C</span>
                </div>

                <div class="status">
                    <div id="tempDot" class="status-dot status-good"></div>
                    <span id="tempStatus">Normal</span>
                </div>
            </div>

            <div class="card">
                <div class="metric-label">
                    Water Level
                </div>

                <div class="metric-value">
                    <span id="dist">--</span>
                    <span class="metric-unit">%</span>
                </div>

                <div class="status">
                    <div id="distDot" class="status-dot status-good"></div>
                    <span id="distStatus">Normal</span>
                </div>
            </div>

            <div class="card">
                <div class="metric-label">
                    Luminosity
                </div>

                <div class="metric-value">
                    <span id="lum">--</span>
                    <span class="metric-unit">%</span>
                </div>

                <div class="status">
                    <div id="lumDot" class="status-dot status-good"></div>
                    <span id="lumStatus">Normal</span>
                </div>
            </div>

        </div>
    </section>

    <section class="section">

        <div class="section-title">
            Actuator Control
        </div>

        <div class="card">

            <div class="control-row">
                <div>
                    Pump Fill
                </div>

                <label class="switch">
                    <input
                        type="checkbox"
                        id="pumpFill"
                        onchange="setPump(1, this.checked)"
                    >

                    <span class="slider"></span>
                </label>
            </div>

            <div class="control-row">
                <div>
                    Pump Drain
                </div>

                <label class="switch">
                    <input
                        type="checkbox"
                        id="pumpDrain"
                        onchange="setPump(2, this.checked)"
                    >

                    <span class="slider"></span>
                </label>
            </div>

        </div>
    </section>

    <section class="section">

        <div class="section-title">
            System State
        </div>

        <div class="grid">

            <div class="card">
                <div class="metric-label">
                    Pump State
                </div>

                <div
                    id="pumpState"
                    class="metric-value"
                    style="font-size:1.4rem;"
                >
                    OFF
                </div>
            </div>

            <div class="card">
                <div class="metric-label">
                    Heater
                </div>

                <div
                    id="heaterState"
                    class="metric-value"
                    style="font-size:1.4rem;"
                >
                    OFF
                </div>
            </div>

        </div>

    </section>

    <section class="section">

        <div class="section-title">
            Event Log
        </div>

        <div class="card">
            <div id="log" class="log">
                System initialized
            </div>
        </div>

    </section>

    <div class="footer">
        ESP32 Aquarium Controller
    </div>

</div>

<script>

const state =
{
    temp: 0,
    dist: 0,
    lum: 0
};

// Pump mode labels matching the backend PumpMode enum (OFF=0, FILL=1, DRAIN=2):
const PUMP_LABELS = ["OFF", "FILL", "DRAIN"];

function log(message)
{
    const logElement =
        document.getElementById("log");

    const timestamp =
        new Date().toLocaleTimeString();

    logElement.innerHTML =
        `[${timestamp}] ${message}<br>` +
        logElement.innerHTML;
}

function setStatus(dotId, textId, level, text)
{
    const dot =
        document.getElementById(dotId);

    dot.className =
        `status-dot status-${level}`;

    document.getElementById(textId)
        .textContent = text;
}

async function fetchTelemetry()
{
    try
    {
        const response =
            await fetch("/xml");

        const text =
            await response.text();

        const xml =
            new DOMParser()
                .parseFromString(
                    text,
                    "text/xml"
                );

        state.temp =
            parseFloat(
                xml.getElementsByTagName("TEMP")[0]
                    .textContent
            );

        state.dist =
            parseFloat(
                xml.getElementsByTagName("DIST")[0]
                    .textContent
            );

        state.lum =
            parseFloat(
                xml.getElementsByTagName("LUM")[0]
                    .textContent
            );

        // Reads pump mode and heater state from the XML response:
        const pumpMode =
            parseInt(
                xml.getElementsByTagName("PUMP")[0]
                    .textContent
            );

        const heaterOn =
            parseInt(
                xml.getElementsByTagName("HEATER")[0]
                    .textContent
            ) === 1;

        document.getElementById("temp")
            .textContent =
            state.temp.toFixed(1);

        document.getElementById("dist")
            .textContent =
            state.dist.toFixed(1);

        document.getElementById("lum")
            .textContent =
            state.lum.toFixed(1);

        // Updates pump state display using the label that matches the received mode index:
        document.getElementById("pumpState")
            .textContent =
            PUMP_LABELS[pumpMode] ?? "OFF";

        // Updates heater state display:
        document.getElementById("heaterState")
            .textContent =
            heaterOn ? "ON" : "OFF";

        // Syncs the pump toggle switches to reflect the actual pump state from the backend:
        document.getElementById("pumpFill").checked =
            pumpMode === 1;

        document.getElementById("pumpDrain").checked =
            pumpMode === 2;

        updateIndicators();
    }
    catch (err)
    {
        log("Communication error");
    }
}

function updateIndicators()
{
    if (state.temp > 26)
    {
        setStatus(
            "tempDot",
            "tempStatus",
            "danger",
            "High"
        );
    }
    else
    {
        setStatus(
            "tempDot",
            "tempStatus",
            "good",
            "Normal"
        );
    }

    if (state.dist > 90 || state.dist < 20)
    {
        setStatus(
            "distDot",
            "distStatus",
            "warn",
            "Attention"
        );
    }
    else
    {
        setStatus(
            "distDot",
            "distStatus",
            "good",
            "Normal"
        );
    }

    if (state.lum < 40)
    {
        setStatus(
            "lumDot",
            "lumStatus",
            "warn",
            "Low"
        );
    }
    else
    {
        setStatus(
            "lumDot",
            "lumStatus",
            "good",
            "Normal"
        );
    }
}

async function setPump(pumpId, enabled)
{
    // Prevents both pumps from being enabled simultaneously:
    if (enabled)
    {
        const otherId = pumpId === 1 ? "pumpDrain" : "pumpFill";

        const otherToggle =
            document.getElementById(otherId);

        if (otherToggle.checked)
        {
            otherToggle.checked = false;

            await fetch("/controlPump",
            {
                method: "POST",

                headers:
                {
                    "Content-Type":
                        "application/json"
                },

                body: JSON.stringify(
                {
                    pumpId: pumpId === 1 ? 2 : 1,
                    state: false
                })
            });
        }
    }

    try
    {
        await fetch("/controlPump",
        {
            method: "POST",

            headers:
            {
                "Content-Type":
                    "application/json"
            },

            body: JSON.stringify(
            {
                pumpId: pumpId,
                state: enabled
            })
        });

        log(
            `Pump ${pumpId} ${
                enabled ? "enabled" : "disabled"
            }`
        );
    }
    catch (err)
    {
        log("Pump control failed");
    }
}

fetchTelemetry();

setInterval(fetchTelemetry, 2000);

</script>

</body>
</html>
)rawliteral";