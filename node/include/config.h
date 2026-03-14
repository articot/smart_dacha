#pragma once
#include <ESP8266WiFi.h>

// -------------------------------------------------------
// WiFi credentials  (same on all nodes)
// -------------------------------------------------------
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// -------------------------------------------------------
// Per-node identity  — CHANGE FOR EACH NODE
// -------------------------------------------------------
#define ROOM_NAME  "Bedroom"          // "Bedroom" | "Kitchen" | "Living Room"

// Static IP — must be unique per node
//   Bedroom     → 192.168.1.101
//   Kitchen     → 192.168.1.102
//   Living Room → 192.168.1.103
const IPAddress STATIC_IP(192, 168, 31, 71);
const IPAddress GATEWAY  (214, 48, 195,   242);
const IPAddress SUBNET   (255, 255, 255, 0);

// -------------------------------------------------------
// DHT22 sensor pin
// -------------------------------------------------------
#define DHT_PIN  D5
