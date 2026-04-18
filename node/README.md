# Smart Dacha — Sensor Node Firmware

Firmware for Wemos D1 Mini (ESP8266) sensor nodes. Each node reads temperature and humidity from a DHT22 sensor and exposes the data over HTTP. Part of the [Smart Dacha](../) multi-room monitoring system.

## Hardware

| Component | Specification |
|-----------|---------------|
| MCU | Wemos D1 Mini (ESP8266) |
| Sensor | DHT22 (AM2302) — temperature & humidity |
| Sensor Pin | `D6` (configurable in `config.h`) |

Three identical nodes are deployed, one per room:

| Room | Static IP |
|------|-----------|
| Bedroom | `192.168.0.101` |
| Kitchen | `192.168.0.102` |
| Living Room | `192.168.0.103` |

## Software Architecture

```
┌─────────────────────────────────────────┐
│           Wemos D1 Mini (ESP8266)       │
│                                         │
│  ┌──────────┐      ┌────────────────┐   │
│  │  DHT22   │──D6──│  main.cpp      │   │
│  │  Sensor  │      │                │   │
│  └──────────┘      │  ┌───────────┐ │   │
│                     │  │ ESP8266   │ │   │
│                     │  │ WebServer │ │   │
│                     │  │ (port 80) │ │   │
│                     │  └───────────┘ │   │
│                     └────────────────┘   │
│                                         │
│  ┌──────────────────────────────────┐   │
│  │  WiFi (STA mode, static IP)     │   │
│  └──────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

### Boot Sequence

1. Initialize serial at 115200 baud
2. Start DHT22 sensor (2s stabilization delay)
3. Configure WiFi in station mode with a static IP
4. Connect to the configured WiFi network (blocks until connected)
5. Register HTTP routes and start the web server

### Main Loop

- Checks WiFi connectivity — restarts the MCU (`ESP.restart()`) if the connection is lost
- Handles incoming HTTP requests via `server.handleClient()`

## HTTP API

### `GET /data`

Returns the current sensor reading.

**Success Response** (`200 OK`):
```json
{
  "room": "Bedroom",
  "temp": 21.5,
  "humidity": 48.2
}
```

| Field | Type | Description |
|-------|------|-------------|
| `room` | string | Room name as configured in `config.h` |
| `temp` | float | Temperature in °C (1 decimal place) |
| `humidity` | float | Relative humidity in % (1 decimal place) |

**Error Response** (`500 Internal Server Error`):
```json
{ "error": "temp sensor read failed" }
```
```json
{ "error": "humidity sensor read failed" }
```

Returned when the DHT22 produces a `NaN` reading.

### `GET /ping`

Health-check endpoint.

**Response** (`200 OK`):
```
OK
```

### `GET /`

Alias for `GET /data`.

## Configuration

All configuration is in [`include/config.h`](include/config.h). Before flashing each node, set the per-node values:

| Define / Variable | Description | Example |
|-------------------|-------------|---------|
| `WIFI_SSID` | WiFi network name | `"TP-Link_38CA"` |
| `WIFI_PASSWORD` | WiFi password | `"07061257"` |
| `ROOM_NAME` | Room identifier (appears in JSON) | `"Bedroom"` |
| `STATIC_IP` | Node's static IP address | `192.168.0.101` |
| `GATEWAY` | Network gateway | `192.168.0.1` |
| `SUBNET` | Subnet mask | `255.255.255.0` |
| `DHT_PIN` | GPIO pin connected to DHT22 data line | `D6` |

An alternative config for a different location is available in [`include/config.krasnoadar.h`](include/config.krasnoadar.h).

## Build & Flash

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Flash (connect one D1 Mini at a time)
pio run --target upload

# Serial monitor
pio device monitor
```

The `platformio.ini` uses `--no-stub` and `--before=no_reset` upload flags for compatibility with certain D1 Mini boards that have unreliable USB-serial connections.

## Project Structure

```
node/
├── platformio.ini          # PlatformIO build configuration
├── include/
│   ├── config.h            # Active configuration (edit per node)
│   └── config.krasnoadar.h # Alternate config for another location
└── src/
    └── main.cpp            # Firmware entry point
```
