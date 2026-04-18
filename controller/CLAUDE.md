# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

See the parent `../CLAUDE.md` for full project context (architecture, emulator, configuration, sync requirements).

## This directory

ESP32 controller firmware (PlatformIO / Arduino C++). All logic lives in two files:
- `src/main.cpp` — web server, poll loop, heater logic, GSM state machine, dashboard HTML
- `include/config.h` — all tunables (WiFi credentials, IPs, thresholds, pins, admin phone)

## Commands

```bash
pio run                      # build
pio run --target upload      # flash ESP32
pio device monitor           # serial @ 115200
```

## Key implementation details

**GSM is disabled at runtime.** The `gsmSerial.begin(...)` block in `setup()` is commented out. The `checkGSM()` / `sendSMS()` functions are compiled in but never triggered. Uncomment the block to re-enable.

**Endpoint naming:** The firmware exposes `GET /data` (not `/api/status`). The parent CLAUDE.md mentions `/api/status` — that name belongs to the emulator side. The JSON schemas must stay in sync regardless of path name.

**`/data` response schema** (must match emulator `GET /api/status`):
```json
{
  "heater": true,
  "mode": "auto",
  "rooms": [
    { "name": "Bedroom", "temp": 21.5, "humidity": 48.0, "online": true, "error": "" }
  ]
}
```
`mode` values: `"auto"` | `"force_on"` | `"force_off"`.

**Heater transition tracking:** `prevBelowState` (global bool) is the only state tracking for SMS alerts. It flips on threshold crossings — not on relay state changes. Do not conflate the two when editing `applyHeaterLogic()`.

**Dashboard is server-rendered HTML** from `buildDashboard()`, then kept live by client-side JS that polls `/data` every 10 s. Heater mode changes use GET redirects (`/heater/on`, `/heater/off`, `/heater/auto` → 303 → `/`).

**`StaticJsonDocument` sizes** are fixed at compile time (`128` bytes for node poll response, `640` bytes for `/data` response). If you add fields to the JSON schema, increase these or switch to `DynamicJsonDocument`.
