# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Smart Dacha is a multi-room temperature/humidity monitoring and gas-heater control system. The repo contains **two parallel implementations of the same system** that must stay behaviorally in sync:

1. **Firmware** (`controller/`, `node/`) — PlatformIO/Arduino C++ targeting real hardware (ESP32 controller + 3× Wemos D1 Mini sensor nodes with DHT22).
2. **Emulator** (`emulator/`) — Docker Compose + Python Flask services that expose the **same HTTP API and implement the same heater/SMS logic**, used for local development without hardware.

When changing behavior in one side (e.g. adding an SMS command, adjusting hysteresis logic, or modifying the `/api/status` schema), the matching change must be made in the other side or the two will drift.

## Architecture

The controller is the only stateful component. Nodes are dumb HTTP sensors.

- **Nodes** expose `GET /data` → `{room, temp, humidity}` and `GET /ping`. In the emulator they also expose `POST /set` and `POST /reset` for scenario testing (no firmware equivalent — only the emulator allows overrides).
- **Controller** polls all 3 nodes on a fixed interval (`POLL_INTERVAL`, default 10s), serves a web dashboard at `/`, exposes `GET /api/status`, and owns heater state.
- **Heater logic** has three modes: `auto`, `force_on`, `force_off`. In `auto`, the relay turns ON when any room drops below `TEMP_THRESHOLD` and OFF when **all** rooms recover above `TEMP_THRESHOLD + HYSTERESIS`. Alerts fire **only on state transitions**, not on every poll — preserve this when editing the poll loop.
- **SMS** (real: SIM800L on ESP32 UART2 / emulator: `POST /sms` mock with `GET /sms/outbox`) accepts commands `STATUS`, `HEATER ON|OFF|AUTO` **only from `ADMIN_PHONE`** — other senders are silently ignored.

Network topology is hard-coded via static IPs (`192.168.1.100` controller, `.101`–`.103` nodes) in the firmware; the emulator uses Docker DNS names (`node-bedroom`, `node-kitchen`, `node-living`).

## Configuration

- **Firmware**: edit `controller/include/config.h` and `node/include/config.h` before flashing. The node's `config.h` is per-device (each of the 3 D1 Minis needs a different `ROOM_NAME` and `STATIC_IP` — you reflash between them).
- **Emulator**: all config via environment variables in `emulator/docker-compose.yml` (see `emulator/README.md` for the full table). No source edits needed to retarget.

## Common commands

### Firmware (PlatformIO)

```bash
cd controller && pio run                    # build controller
cd controller && pio run --target upload    # flash ESP32
cd node && pio run --target upload          # flash one D1 Mini (repeat per node)
pio device monitor                          # serial monitor @ 115200
```

Both `platformio.ini` files set `build_dir`/`libdeps_dir` to `.pio/...` relative to the project folder (controller and node each get their own `.pio/` — already gitignored). Note also that `node/platformio.ini` hard-codes `upload_port = COM7` with `esptool --no-stub --before=no_reset` flags — these are specific to the author's flaky D1 Mini and should not be "cleaned up" without reason.

### Emulator (Docker)

```bash
cd emulator
docker compose up --build -d     # start all 4 services
docker compose down              # stop
docker compose logs -f controller
docker stop node-kitchen         # simulate a node going offline
```

Dashboard: http://localhost:8080. Node ports: 8081 (bedroom), 8082 (kitchen), 8083 (living). There is **no test suite** — verify changes via the scenario recipes in `emulator/README.md` (temperature drop → alert, recovery → info, offline node, SMS auth).

## Tests

There are no unit tests in this repo. "Testing" means running the emulator and exercising the scenarios in `emulator/README.md`. If adding tests, put them under the implementation they cover and do not assume a test runner is configured.
