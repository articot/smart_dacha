#pragma once
#include <WiFi.h>

// -------------------------------------------------------
// WiFi credentials
// -------------------------------------------------------
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// -------------------------------------------------------
// Controller static IP
// -------------------------------------------------------
const IPAddress STATIC_IP(192, 168, 1, 100);
const IPAddress GATEWAY  (192, 168, 1,   1);
const IPAddress SUBNET   (255, 255, 255, 0);

// -------------------------------------------------------
// Sensor node IPs  (must match each node's config.h)
// -------------------------------------------------------
#define NODE1_IP  "192.168.1.101"   // Bedroom
#define NODE2_IP  "192.168.1.102"   // Kitchen
#define NODE3_IP  "192.168.1.103"   // Living Room
#define NUM_ROOMS 3

// -------------------------------------------------------
// Relay
// -------------------------------------------------------
#define RELAY_PIN  26
#define RELAY_ON   HIGH   // Change to LOW if your relay module is active-low
#define RELAY_OFF  LOW

// -------------------------------------------------------
// Heater auto-control thresholds
// -------------------------------------------------------
#define TEMP_THRESHOLD  20.0f   // °C — heater turns ON when any room drops below this
#define HYSTERESIS       0.5f   // °C — heater turns OFF only when all rooms exceed threshold + hysteresis

// -------------------------------------------------------
// Polling interval
// -------------------------------------------------------
#define POLL_INTERVAL_MS  10000UL   // 10 seconds

// -------------------------------------------------------
// GSM module (SIM800L) — connected to UART2
// -------------------------------------------------------
#define GSM_RX_PIN    16            // ESP32 RX2 ← SIM800L TX
#define GSM_TX_PIN    17            // ESP32 TX2 → SIM800L RX
#define GSM_BAUD      9600

// Only this number may send commands and will receive all alerts
#define ADMIN_PHONE   "+1234567890"
