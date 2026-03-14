# Smart Dacha — Temperature Monitoring & Heater Control

A fully Arduino-based smart home system for monitoring temperature and humidity across multiple rooms and automatically controlling a gas heater. Built with PlatformIO in VS Code.

---

## Architecture

```
Browser / Phone
      │  HTTP GET /
      ▼
  [ESP32 Controller]  ──── GPIO 26 ────▶  Relay ──▶  Gas Heater
  192.168.1.100
  + SIM800L GSM
      │
      ├─ GET /data ──▶  [D1 Mini — Bedroom]      192.168.1.101
      ├─ GET /data ──▶  [D1 Mini — Kitchen]       192.168.1.102
      └─ GET /data ──▶  [D1 Mini — Living Room]   192.168.1.103
```

---

## Hardware

| Qty | Component | Role |
|-----|-----------|------|
| 3×  | Wemos D1 Mini (ESP8266) | Sensor node — one per room |
| 3×  | DHT22 + 10 kΩ resistor | Temperature + humidity sensor |
| 1×  | ESP32 DevKit | Controller, web dashboard, heater logic |
| 1×  | SIM800L GSM module | SMS alerts & remote control |
| 1×  | 5 V relay module (1-channel) | Switches the gas heater |
| —   | 5 V USB power supplies | Power for all boards |

### Wiring — SIM800L → ESP32

| SIM800L | ESP32 |
|---------|-------|
| TX | GPIO 16 (RX2) |
| RX | GPIO 17 (TX2) |
| VCC | 3.7–4.2 V (dedicated supply) |
| GND | GND |

> **Note:** SIM800L requires up to 2 A peak current. Do **not** power it from the ESP32 3.3 V pin — use a dedicated LiPo or a quality 4 V supply.

### Wiring — Relay → ESP32

| Relay module | ESP32 |
|---|---|
| IN | GPIO 26 |
| VCC | 5 V |
| GND | GND |

---

## Project Structure

```
smart dacha/
├── README.md
├── controller/                  ← Flash to ESP32
│   ├── platformio.ini
│   ├── include/
│   │   └── config.h             ← WiFi, IPs, thresholds, GSM phone number
│   └── src/
│       └── main.cpp
└── node/                        ← Flash to each D1 Mini (×3)
    ├── platformio.ini
    ├── include/
    │   └── config.h             ← WiFi, per-node static IP, room name
    └── src/
        └── main.cpp
```

---

## Configuration

### 1. WiFi (both projects)

Edit `include/config.h` in **both** `controller/` and `node/`:

```cpp
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"
```

### 2. Sensor nodes — flash each D1 Mini separately

Before each flash, edit `node/include/config.h`:

| Node | `ROOM_NAME` | `STATIC_IP` |
|------|-------------|-------------|
| 1 | `"Bedroom"` | `192.168.1.101` |
| 2 | `"Kitchen"` | `192.168.1.102` |
| 3 | `"Living Room"` | `192.168.1.103` |

### 3. Controller — `controller/include/config.h`

```cpp
#define ADMIN_PHONE     "+1234567890"  // Only this number can send commands / receive alerts

#define TEMP_THRESHOLD  20.0f          // °C — heater turns ON below this
#define HYSTERESIS       0.5f          // °C — heater turns OFF above threshold + hysteresis

#define RELAY_PIN       26
#define RELAY_ON        HIGH           // Change to LOW if your relay is active-low
```

### 4. Router — assign static IPs

In your router's DHCP settings, reserve the following IPs by MAC address:

| Device | IP |
|--------|----|
| ESP32 (controller) | `192.168.1.100` |
| D1 Mini #1 (Bedroom) | `192.168.1.101` |
| D1 Mini #2 (Kitchen) | `192.168.1.102` |
| D1 Mini #3 (Living Room) | `192.168.1.103` |

---

## Building & Flashing

Open the `controller/` or `node/` folder in VS Code with PlatformIO installed.

```
PlatformIO sidebar → Build   (Ctrl+Alt+B)
PlatformIO sidebar → Upload  (Ctrl+Alt+U)
```

Or via terminal:
```bash
cd controller
pio run --target upload

cd ../node
pio run --target upload
```

---

## Web Dashboard

Open `http://192.168.1.100` in any browser on the same network.

- **Room cards** — current temperature (°C) and humidity (% RH); greyed out if node is offline
- **Heater panel**
  - **Auto** mode — heater is controlled automatically by temperature thresholds
  - **Manual** mode — reveals Turn ON / Turn OFF buttons for direct control
- Page auto-refreshes every 10 seconds

**JSON API:** `GET http://192.168.1.100/api/status`

```json
{
  "heater": true,
  "mode": "auto",
  "rooms": [
    { "name": "Bedroom",     "temp": 19.8, "humidity": 47.0, "online": true },
    { "name": "Kitchen",     "temp": 21.2, "humidity": 55.1, "online": true },
    { "name": "Living Room", "temp": 20.5, "humidity": 50.3, "online": true }
  ]
}
```

---

## SMS Control

Send SMS from `ADMIN_PHONE` to the SIM card inserted in the SIM800L:

| Message | Action |
|---------|--------|
| `STATUS` | Replies with all rooms' temp/humidity + heater state and mode |
| `HEATER ON` | Forces heater ON — replies with confirmation |
| `HEATER OFF` | Forces heater OFF — replies with confirmation |
| `HEATER AUTO` | Enables automatic mode — replies with confirmation |

Commands from any other number are silently ignored.

---

## Automatic SMS Alerts

The controller sends alerts to `ADMIN_PHONE` automatically:

| Trigger | Message |
|---------|---------|
| Any room drops **below** `TEMP_THRESHOLD` | `ALERT: Temp dropped below 20.0C!` + all room readings + `Heater: ON (Auto)` |
| All rooms recover **above** `TEMP_THRESHOLD + HYSTERESIS` | `INFO: Temp restored above 20.5C.` + all room readings + `Heater: OFF (Auto)` |

Alerts fire only on **state transitions** — not repeatedly every poll cycle.

---

## Dependencies

### Controller (espressif32)
- `bblanchon/ArduinoJson @ ^6`
- `HTTPClient`, `WebServer`, `WiFi` — bundled with the ESP32 Arduino core

### Node (espressif8266)
- `adafruit/DHT sensor library @ ^1`
- `adafruit/Adafruit Unified Sensor @ ^1`
- `ESP8266WebServer`, `ESP8266WiFi` — bundled with the ESP8266 Arduino core
