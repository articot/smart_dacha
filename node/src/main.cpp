#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include "config.h"

DHT dht(DHT_PIN, DHT22);
ESP8266WebServer server(80);

// -------------------------------------------------------
// GET /data  →  {"room":"Bedroom","temp":21.5,"humidity":48.2}
// -------------------------------------------------------
void handleData()
{
    Serial.println("[data] Requesting sensor data...");

    // float temp = 22;
    // float humidity = 48;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    Serial.println("[data] Sensor data: temp=" + String(temperature) + "°C, humidity=" + String(humidity) + "%");

    if (isnan(temperature))
    {
        server.send(500, "application/json", "{\"error\":\" temp sensor read failed\"}");
        return;
    }

    if (isnan(humidity))
    {
        server.send(500, "application/json", "{\"error\":\"humidity sensor read failed\"}");
        return;
    }

    String json = "{\"room\":\"" + String(ROOM_NAME) + "\","
                                                       "\"temp\":" +
                  String(temperature, 1) + ","
                                    "\"humidity\":" +
                  String(humidity, 1) + "}";

    server.send(200, "application/json", json);
}

// -------------------------------------------------------
// GET /ping  →  simple health-check
// -------------------------------------------------------
void handlePing()
{
    server.send(200, "text/plain", "OK");
}

// -------------------------------------------------------
// Setup & loop
// -------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println("Starting node [" + String(ROOM_NAME) + "]...");

    dht.begin();
    delay(2000); // DHT22 needs time to stabilize

    Serial.println("DHT started...");

    WiFi.mode(WIFI_STA);
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nNode [" + String(ROOM_NAME) + "] online at http://" + WiFi.localIP().toString());

    server.on("/", HTTP_GET, handleData);
    server.on("/data", HTTP_GET, handleData);
    server.on("/ping", HTTP_GET, handlePing);
    server.begin();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi lost!");
        ESP.restart();
    }
    server.handleClient();
}
