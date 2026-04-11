#pragma once
#include <ESP8266WiFi.h>

// -------------------------------------------------------
// WiFi credentials  (same on all nodes)
// -------------------------------------------------------
#define WIFI_SSID     "RT-GPON-3116"
#define WIFI_PASSWORD "GE4TG6G2"

// -------------------------------------------------------
// Per-node identity  — CHANGE FOR EACH NODE
// -------------------------------------------------------
#define ROOM_NAME  "Bedroom"          // "Bedroom" | "Kitchen" | "Living Room"

// Static IP — must be unique per node
//   Bedroom     → 192.168.0.101
//   Kitchen     → 192.168.0.102
//   Living Room → 192.168.0.103
const IPAddress STATIC_IP(192, 168, 0, 101);
const IPAddress GATEWAY  (192, 168, 0, 1);
const IPAddress SUBNET   (255, 255, 255, 0);

// -------------------------------------------------------
// DHT22 sensor pin
// -------------------------------------------------------
#define DHT_PIN  D5
