# Smart Dacha — Emulator

Docker-based local emulation of the entire Smart Dacha system. Replaces real ESP32/ESP8266 hardware with Python Flask containers that expose the same HTTP API and implement the same heater logic, threshold alerts, and SMS handling.

---

## Quick Start

```powershell
cd emulator
docker compose up --build -d
```

Open the dashboard: http://localhost:8080

Stop everything:

```powershell
docker compose down
```

Rebuild after code changes:

```powershell
docker compose up --build -d
```

---

## Containers

| Container | Image source | Port | Description |
|-----------|-------------|------|-------------|
| `controller` | `emulator/controller/` | **8080** | Polls all 3 nodes, serves the web dashboard, heater logic, SMS mock |
| `node-bedroom` | `emulator/node/` | **8081** | Simulates bedroom DHT22 sensor |
| `node-kitchen` | `emulator/node/` | **8082** | Simulates kitchen DHT22 sensor |
| `node-living` | `emulator/node/` | **8083** | Simulates living room DHT22 sensor |

---

## Environment Variables

### Node containers

| Variable | Default | Description |
|----------|---------|-------------|
| `ROOM_NAME` | `"Unknown"` | Name shown in JSON responses |
| `INIT_TEMP` | `21.0` | Starting temperature (°C) |
| `INIT_HUMIDITY` | `50.0` | Starting humidity (%) |
| `DRIFT` | `"true"` | Enable slow random drift (±0.15°C / 5s) |

### Controller container

| Variable | Default | Description |
|----------|---------|-------------|
| `NODE1_URL` | `http://node-bedroom:80` | URL of node 1 |
| `NODE2_URL` | `http://node-kitchen:80` | URL of node 2 |
| `NODE3_URL` | `http://node-living:80` | URL of node 3 |
| `TEMP_THRESHOLD` | `20.0` | Auto-heater ON threshold (°C) |
| `HYSTERESIS` | `0.5` | Heater OFF at threshold + hysteresis |
| `POLL_INTERVAL` | `10` | Polling interval (seconds) |
| `ADMIN_PHONE` | `+1234567890` | Phone number for SMS commands/alerts |

---

## API Reference

### Node Endpoints (ports 8081–8083)

#### `GET /data` — Read sensor values

```powershell
Invoke-RestMethod http://localhost:8081/data
```

Response:

```json
{ "room": "Bedroom", "temp": 21.3, "humidity": 48.7 }
```

#### `GET /ping` — Health check

```powershell
Invoke-RestMethod http://localhost:8081/ping
```

Response: `OK`

#### `POST /set` — Override sensor values (testing)

Set specific temperature and/or humidity to simulate scenarios:

```powershell
# Set bedroom temp to 15°C (below threshold — will trigger heater + alert)
Invoke-RestMethod -Method Post -Uri http://localhost:8081/set `
    -ContentType "application/json" `
    -Body '{"temp": 15.0}'

# Set only humidity
Invoke-RestMethod -Method Post -Uri http://localhost:8082/set `
    -ContentType "application/json" `
    -Body '{"humidity": 90.0}'

# Set both
Invoke-RestMethod -Method Post -Uri http://localhost:8083/set `
    -ContentType "application/json" `
    -Body '{"temp": 25.0, "humidity": 30.0}'
```

#### `POST /reset` — Return to random drift

```powershell
Invoke-RestMethod -Method Post -Uri http://localhost:8081/reset
```

---

### Controller Endpoints (port 8080)

#### `GET /` — Web Dashboard

Open in browser: http://localhost:8080

Features:
- Room cards with temperature and humidity
- Heater state (ON/OFF)
- Auto/Manual mode radio buttons
- Manual Turn ON / Turn OFF buttons
- Auto-refreshes every 10 seconds

#### `GET /data` — JSON status of entire system

```powershell
Invoke-RestMethod http://localhost:8080/data | ConvertTo-Json -Depth 3
```

Response:

```json
{
  "heater": true,
  "mode": "auto",
  "rooms": [
    { "name": "Bedroom",     "temp": 21.5, "humidity": 48.0, "online": true,  "error": "" },
    { "name": "Kitchen",     "temp": 20.2, "humidity": 55.0, "online": true,  "error": "" },
    { "name": "Living Room", "temp": 19.5, "humidity": 50.0, "online": false, "error": "Connection refused" }
  ]
}
```

#### `GET /heater/on` — Force heater ON

```powershell
Invoke-RestMethod http://localhost:8080/heater/on
```

#### `GET /heater/off` — Force heater OFF

```powershell
Invoke-RestMethod http://localhost:8080/heater/off
```

#### `GET /heater/auto` — Set heater to auto mode

```powershell
Invoke-RestMethod http://localhost:8080/heater/auto
```

---

### SMS Mock Endpoints (port 8080)

#### `POST /sms` — Simulate incoming SMS

```powershell
# Send STATUS command
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms `
    -ContentType "application/json" `
    -Body '{"from": "+1234567890", "text": "STATUS"}'

# Force heater ON via SMS
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms `
    -ContentType "application/json" `
    -Body '{"from": "+1234567890", "text": "HEATER ON"}'

# Force heater OFF via SMS
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms `
    -ContentType "application/json" `
    -Body '{"from": "+1234567890", "text": "HEATER OFF"}'

# Set heater to auto via SMS
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms `
    -ContentType "application/json" `
    -Body '{"from": "+1234567890", "text": "HEATER AUTO"}'

# Send from wrong number (should be ignored)
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms `
    -ContentType "application/json" `
    -Body '{"from": "+9999999999", "text": "STATUS"}'
```

#### `GET /sms/outbox` — View all sent SMS messages

```powershell
Invoke-RestMethod http://localhost:8080/sms/outbox | ConvertTo-Json -Depth 3
```

Response:

```json
[
  {
    "to": "+1234567890",
    "text": "Smart Dacha Status:\nBedroom: 21.5C, 48.0%RH\nKitchen: 20.2C, 55.0%RH\nLiving Room: 19.5C, 50.0%RH\nHeater: ON (Auto)",
    "timestamp": "2026-03-08T14:35:28.584074"
  }
]
```

#### `POST /sms/outbox/clear` — Clear the SMS outbox

```powershell
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms/outbox/clear
```

---

## Testing Scenarios

### 1. Verify all nodes are online

```powershell
Invoke-RestMethod http://localhost:8081/data | ConvertTo-Json
Invoke-RestMethod http://localhost:8082/data | ConvertTo-Json
Invoke-RestMethod http://localhost:8083/data | ConvertTo-Json
```

### 2. Simulate temperature drop below threshold (trigger heater + SMS alert)

```powershell
# First set heater to auto and clear outbox
Invoke-RestMethod http://localhost:8080/heater/auto
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms/outbox/clear

# Set all rooms above threshold
Invoke-RestMethod -Method Post -Uri http://localhost:8081/set -ContentType "application/json" -Body '{"temp": 22.0}'
Invoke-RestMethod -Method Post -Uri http://localhost:8082/set -ContentType "application/json" -Body '{"temp": 22.0}'
Invoke-RestMethod -Method Post -Uri http://localhost:8083/set -ContentType "application/json" -Body '{"temp": 22.0}'

# Wait for a poll cycle
Start-Sleep 12

# Now drop bedroom below threshold
Invoke-RestMethod -Method Post -Uri http://localhost:8081/set -ContentType "application/json" -Body '{"temp": 15.0}'

# Wait for the controller to poll and detect
Start-Sleep 12

# Check: heater should be ON, alert SMS should be in outbox
Invoke-RestMethod http://localhost:8080/api/status | ConvertTo-Json -Depth 3
Invoke-RestMethod http://localhost:8080/sms/outbox | ConvertTo-Json -Depth 3
```

### 3. Simulate temperature recovery (heater off + recovery SMS)

```powershell
# Raise all rooms above threshold + hysteresis (20.0 + 0.5 = 20.5)
Invoke-RestMethod -Method Post -Uri http://localhost:8081/set -ContentType "application/json" -Body '{"temp": 22.0}'
Invoke-RestMethod -Method Post -Uri http://localhost:8082/set -ContentType "application/json" -Body '{"temp": 22.0}'
Invoke-RestMethod -Method Post -Uri http://localhost:8083/set -ContentType "application/json" -Body '{"temp": 22.0}'

# Wait for poll cycle
Start-Sleep 12

# Heater should be OFF, recovery SMS in outbox
Invoke-RestMethod http://localhost:8080/api/status | ConvertTo-Json -Depth 3
Invoke-RestMethod http://localhost:8080/sms/outbox | ConvertTo-Json -Depth 3
```

### 4. Simulate a node going offline

```powershell
# Stop the kitchen node
docker stop node-kitchen

# Wait for poll
Start-Sleep 12

# Kitchen should show online: false
Invoke-RestMethod http://localhost:8080/api/status | ConvertTo-Json -Depth 3

# Bring it back
docker start node-kitchen
```

### 5. Test SMS security (wrong phone number)

```powershell
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms/outbox/clear

# Send from unauthorized number
Invoke-RestMethod -Method Post -Uri http://localhost:8080/sms `
    -ContentType "application/json" `
    -Body '{"from": "+9999999999", "text": "HEATER ON"}'

# Outbox should be empty — command was ignored
Invoke-RestMethod http://localhost:8080/sms/outbox | ConvertTo-Json -Depth 3
```

### 6. Test manual heater control via web

```powershell
# Force ON
Invoke-RestMethod http://localhost:8080/heater/on
Invoke-RestMethod http://localhost:8080/api/status | ConvertTo-Json -Depth 3
# → heater: true, mode: force_on

# Force OFF
Invoke-RestMethod http://localhost:8080/heater/off
Invoke-RestMethod http://localhost:8080/api/status | ConvertTo-Json -Depth 3
# → heater: false, mode: force_off

# Back to auto
Invoke-RestMethod http://localhost:8080/heater/auto
Invoke-RestMethod http://localhost:8080/api/status | ConvertTo-Json -Depth 3
# → mode: auto
```

---

## Architecture

```
localhost:8080 ─────────────────────────────────────────────
  │  Controller (Flask)                                    
  │  - Polls nodes every 10s                               
  │  - Serves dashboard at /                               
  │  - Heater logic with threshold + hysteresis            
  │  - SMS mock: POST /sms → process → /sms/outbox        
  │                                                        
  ├── http://node-bedroom:80/data  →  localhost:8081       
  ├── http://node-kitchen:80/data  →  localhost:8082       
  └── http://node-living:80/data   →  localhost:8083       
────────────────────────────────────────────────────────────
  Each node:                                               
  - Returns {"room","temp","humidity"}                     
  - Drifts ±0.15°C every 5s                               
  - POST /set to override, POST /reset to resume drift    
```
