# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for the sensor nodes — 3× Wemos D1 Mini (ESP8266) boards, each running an HTTP server that exposes temperature/humidity from a DHT22 sensor. All three nodes share one codebase; identity is set in `include/config.h` before flashing.

## Commands

```bash
pio run                        # build
pio run --target upload        # flash (see upload quirks below)
pio device monitor             # serial @ 115200
```

Upload flags in `platformio.ini` (`--no-stub --before=no_reset --connect-attempts=20`) are intentional for a flaky D1 Mini — do not remove them.

## Per-node configuration (`include/config.h`)

Must change for each physical device before flashing:

| Node | `ROOM_NAME` | `STATIC_IP` |
|------|-------------|-------------|
| 1 | `"Bedroom"` | `192.168.0.101` |
| 2 | `"Kitchen"` | `192.168.0.102` |
| 3 | `"Living Room"` | `192.168.0.103` |

`DHT_PIN` is `D6` on all nodes.

## HTTP API

| Endpoint | Response |
|----------|----------|
| `GET /data` (also `/`) | `{"room":"…","temp":21.5,"humidity":48.2}` |
| `GET /ping` | `OK` |

On DHT22 read failure, `/data` returns HTTP 500 with a JSON error. The emulator's nodes also expose `POST /set` and `POST /reset` — no firmware equivalent.

## Key behavior

- WiFi loss triggers `ESP.restart()` — no reconnect loop.
- DHT22 gets a 2 s stabilization delay in `setup()` before WiFi init starts.
- `loop()` only calls `server.handleClient()`; no background tasks.
