#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include "config.h"

DHT dht(DHT_PIN, DHT22);
ESP8266WebServer server(8080);

// -------------------------------------------------------
// GET /data  →  {"room":"Bedroom","temp":21.5,"humidity":48.2}
// -------------------------------------------------------
void handleData()
{
    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temp))
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
                  String(temp, 1) + ","
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
    dht.begin();

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
