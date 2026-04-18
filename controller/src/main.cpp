#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

HardwareSerial gsmSerial(2);

// -------------------------------------------------------
// Data model
// -------------------------------------------------------
struct RoomData
{
    const char *name;
    const char *ip;
    float temp;
    float humidity;
    bool online;
    String error;
};

RoomData rooms[NUM_ROOMS] = {
    {"Bedroom", NODE1_IP, 0.0f, 0.0f, false},
    {"Kitchen", NODE2_IP, 0.0f, 0.0f, false},
    {"Living Room", NODE3_IP, 0.0f, 0.0f, false},
};

enum HeaterMode
{
    AUTO,
    FORCE_ON,
    FORCE_OFF
};

HeaterMode heaterMode = AUTO;
bool relayState = false;
unsigned long lastPoll = 0;
bool prevBelowState = false; // tracks threshold crossing for SMS alerts

// GSM serial state machine
enum GsmRxState
{
    GSM_IDLE,
    GSM_WAIT_BODY
};
GsmRxState gsmRxState = GSM_IDLE;
String gsmLineBuf = "";
String smsSender = "";

WebServer server(80);

// -------------------------------------------------------
// Relay helpers
// -------------------------------------------------------
void setRelay(bool on)
{
    relayState = on;
    digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

// -------------------------------------------------------
// GSM helpers
// -------------------------------------------------------
void gsmCmd(const char *cmd, unsigned long waitMs = 500)
{
    gsmSerial.println(cmd);
    delay(waitMs);
    while (gsmSerial.available())
        Serial.write(gsmSerial.read());
}

void sendSMS(const char *phone, const String &msg)
{
    String at = "AT+CMGS=\"";
    at += phone;
    at += "\"";
    gsmSerial.println(at);
    delay(300);
    gsmSerial.print(msg);
    gsmSerial.write(0x1A); // Ctrl+Z — ends the message
    delay(3000);
}

String buildStatusSMS()
{
    String msg = "Smart Dacha Status:\n";
    for (int i = 0; i < NUM_ROOMS; i++)
    {
        msg += String(rooms[i].name) + ": ";
        if (rooms[i].online)
            msg += String(rooms[i].temp, 1) + "C, " + String(rooms[i].humidity, 1) + "%RH\n";
        else
            msg += "OFFLINE\n";
    }
    msg += "Heater: ";
    msg += relayState ? "ON" : "OFF";
    msg += " (";
    msg += (heaterMode == AUTO) ? "Auto" : (heaterMode == FORCE_ON) ? "Force ON"
                                                                    : "Force OFF";
    msg += ")";
    return msg;
}

void processSMS(const String &sender, String text)
{
    // Security: reject commands from anyone except the configured admin number
    if (sender != String(ADMIN_PHONE))
        return;

    text.trim();
    text.toUpperCase();

    if (text == "STATUS")
    {
        sendSMS(ADMIN_PHONE, buildStatusSMS());
    }
    else if (text == "HEATER ON")
    {
        heaterMode = FORCE_ON;
        setRelay(true);
        sendSMS(ADMIN_PHONE, "Heater forced ON.");
    }
    else if (text == "HEATER OFF")
    {
        heaterMode = FORCE_OFF;
        setRelay(false);
        sendSMS(ADMIN_PHONE, "Heater forced OFF.");
    }
    else if (text == "HEATER AUTO")
    {
        heaterMode = AUTO;
        sendSMS(ADMIN_PHONE, "Heater set to AUTO mode.");
    }
    else
    {
        sendSMS(ADMIN_PHONE, "Unknown cmd. Valid:\nSTATUS\nHEATER ON\nHEATER OFF\nHEATER AUTO");
    }
}

// Non-blocking: reads SIM800L serial and handles incoming +CMT: SMS events
void checkGSM()
{
    while (gsmSerial.available())
    {
        char c = gsmSerial.read();
        if (c == '\r')
            continue;
        if (c == '\n')
        {
            String line = gsmLineBuf;
            gsmLineBuf = "";
            if (line.startsWith("+CMT:"))
            {
                // +CMT: "+79991234567","","yy/mm/dd,hh:mm:ss+zz"
                int q1 = line.indexOf('"');
                int q2 = line.indexOf('"', q1 + 1);
                smsSender = (q1 >= 0 && q2 > q1) ? line.substring(q1 + 1, q2) : "";
                gsmRxState = GSM_WAIT_BODY;
            }
            else if (gsmRxState == GSM_WAIT_BODY && line.length() > 0)
            {
                gsmRxState = GSM_IDLE;
                processSMS(smsSender, line);
            }
        }
        else
        {
            gsmLineBuf += c;
        }
    }
}

// -------------------------------------------------------
// Poll all sensor nodes
// -------------------------------------------------------
void pollNodes()
{
    HTTPClient http;
    for (int i = 0; i < NUM_ROOMS; i++)
    {
        try
        {
            String url = "http://" + String(rooms[i].ip) + "/data";
            http.begin(url);
            http.setTimeout(3000);
            int code = http.GET();

            StaticJsonDocument<128> doc;
            DeserializationError deserializationError = deserializeJson(doc, http.getString());

            if (code == HTTP_CODE_OK)
            {
                if (!deserializationError)
                {
                    rooms[i].temp = doc["temp"].as<float>();
                    rooms[i].humidity = doc["humidity"].as<float>();
                    rooms[i].online = true;
                    rooms[i].error = "";
                }
                else
                {
                    rooms[i].online = false;
                    rooms[i].error = deserializationError.c_str();
                }
            }
            else
            {
                rooms[i].online = false;
                String statusCode = code > 0 ? String(code) : "";

                if (!deserializationError)
                {
                    rooms[i].error = statusCode + " - " + doc["error"].as<String>();
                }
                else
                {
                    rooms[i].error = statusCode + " - " + deserializationError.c_str();
                }

                Serial.println("Error polling " + String(rooms[i].name) + ": " + rooms[i].error);
            }
            http.end();
        }
        catch (const std::exception &e)
        {
            rooms[i].online = false;
            rooms[i].error = String(e.what());
            Serial.println("Error polling " + String(rooms[i].name) + ": " + rooms[i].error);
        }
    }
}

// -------------------------------------------------------
// Heater threshold logic
// -------------------------------------------------------
void applyHeaterLogic()
{
    if (heaterMode == FORCE_ON)
    {
        setRelay(true);
        return;
    }
    if (heaterMode == FORCE_OFF)
    {
        setRelay(false);
        return;
    }

    // AUTO mode
    bool anyBelowThreshold = false;
    bool allAboveHysteresis = true;

    for (int i = 0; i < NUM_ROOMS; i++)
    {
        if (!rooms[i].online)
            continue;
        if (rooms[i].temp < TEMP_THRESHOLD)
            anyBelowThreshold = true;
        if (rooms[i].temp <= TEMP_THRESHOLD + HYSTERESIS)
            allAboveHysteresis = false;
    }

    if (!relayState && anyBelowThreshold)
        setRelay(true);
    if (relayState && allAboveHysteresis)
        setRelay(false);

    // SMS alerts on threshold state transitions
    if (!prevBelowState && anyBelowThreshold)
    {
        prevBelowState = true;
        String msg = "ALERT: Temp dropped below " + String(TEMP_THRESHOLD, 1) + "C!\n";
        for (int i = 0; i < NUM_ROOMS; i++)
            if (rooms[i].online)
                msg += String(rooms[i].name) + ": " + String(rooms[i].temp, 1) + "C\n";
        msg += "Heater: ON (Auto)";
        sendSMS(ADMIN_PHONE, msg);
    }
    else if (prevBelowState && allAboveHysteresis)
    {
        prevBelowState = false;
        String msg = "INFO: Temp restored above " + String(TEMP_THRESHOLD + HYSTERESIS, 1) + "C.\n";
        for (int i = 0; i < NUM_ROOMS; i++)
            if (rooms[i].online)
                msg += String(rooms[i].name) + ": " + String(rooms[i].temp, 1) + "C\n";
        msg += "Heater: OFF (Auto)";
        sendSMS(ADMIN_PHONE, msg);
    }
}

// -------------------------------------------------------
// Dashboard HTML
// -------------------------------------------------------
String buildDashboard()
{
    bool isAuto = (heaterMode == AUTO);

    String html = F(R"(<!DOCTYPE html><html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>Smart Dacha</title>
  <style>
    body{font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:20px;margin:0}
    h1{color:#e94560;margin-bottom:4px}
    .subtitle{color:#6c6c8a;font-size:.85em;margin-bottom:28px}
    .grid{display:flex;flex-wrap:wrap;justify-content:center;gap:16px;margin-bottom:28px}
    .card{background:#16213e;border-radius:14px;padding:22px 28px;min-width:140px}
    .card.offline{opacity:.35}
    .err{font-size:.72em;color:#e74c3c;margin-top:4px;word-break:break-all}
    .room{font-size:.8em;color:#7a7aaa;margin-bottom:6px;text-transform:uppercase;letter-spacing:.05em}
    .temp{font-size:2.6em;font-weight:700;color:#e94560;line-height:1}
    .hum{font-size:1em;color:#00b4d8;margin-top:6px}
    .panel{background:#16213e;border-radius:14px;padding:20px;max-width:320px;margin:0 auto}
    .panel h2{margin:0 0 14px;font-size:1em;text-transform:uppercase;letter-spacing:.05em;color:#7a7aaa}
    .state{font-size:1.3em;font-weight:600;margin-bottom:16px}
    .on{color:#2ecc71} .off{color:#e74c3c}
    .mode-switch{display:flex;justify-content:center;gap:12px;margin-bottom:16px}
    .radio-lbl{display:flex;align-items:center;gap:8px;cursor:pointer;font-size:.95em;background:#0f3460;padding:9px 20px;border-radius:8px;transition:background .2s}
    .radio-lbl:hover{background:#1a4a80}
    .radio-lbl input{accent-color:#e94560;width:16px;height:16px;cursor:pointer}
    .btns{display:flex;gap:8px;justify-content:center;flex-wrap:wrap}
    .btn{border:none;border-radius:8px;padding:9px 28px;font-size:.95em;cursor:pointer;font-weight:600}
    .btn-on {background:#2ecc71;color:#000}
    .btn-off{background:#e74c3c;color:#fff}
    .thr{font-size:.8em;color:#555580;margin-top:14px}
    .poll-status{color:#6c6c8a;font-size:.75em;margin-top:6px;margin-bottom:28px;transition:color .3s}
    .poll-status.fresh{color:#2ecc71}
  </style>
</head>
<body>
<h1>Smart Dacha</h1>
<div class='subtitle'>updates every 10 s</div>
<div class='poll-status' id='poll-status'>waiting for first update&hellip;</div>
<div class='grid' id='rooms-grid'>)");

    for (int i = 0; i < NUM_ROOMS; i++)
    {
        html += "<div class='card" + String(rooms[i].online ? "" : " offline") + "' id='card-" + String(i) + "'>";
        html += "<div class='room'>" + String(rooms[i].name) + "</div>";
        if (rooms[i].online)
        {
            html += "<div class='temp' id='temp-" + String(i) + "'>" + String(rooms[i].temp, 1) + "&deg;</div>";
            html += "<div class='hum' id='hum-" + String(i) + "'>" + String(rooms[i].humidity, 1) + "% RH</div>";
            html += "<div class='err' id='err-" + String(i) + "'></div>";
        }
        else
        {
            html += "<div class='temp' id='temp-" + String(i) + "'>--</div>";
            html += "<div class='hum' id='hum-" + String(i) + "'>offline</div>";
            html += "<div class='err' id='err-" + String(i) + "'>" + rooms[i].error + "</div>";
        }
        html += "</div>";
    }

    html += "</div>"; // close .grid

    // Heater panel
    html += "<div class='panel'><h2>Gas Heater</h2>";
    html += "<div class='state'>State: <span id='heater-state' class='" + String(relayState ? "on'>ON" : "off'>OFF") + "</span></div>";

    // Mode radio buttons
    html += "<div class='mode-switch'>";
    html += "<label class='radio-lbl'><input type='radio' name='hmode' id='mode-auto' value='auto' onchange=\"switchMode(this.value)\"";
    if (isAuto)
        html += " checked";
    html += "> Auto</label>";
    html += "<label class='radio-lbl'><input type='radio' name='hmode' id='mode-manual' value='manual' onchange=\"switchMode(this.value)\"";
    if (!isAuto)
        html += " checked";
    html += "> Manual</label>";
    html += "</div>";

    // Manual ON/OFF buttons — hidden by default, shown by JS when manual is selected
    html += "<div id='mbts' class='btns' style='display:none'>";
    html += "<form action='/heater/on'  method='get'><button class='btn btn-on' >Turn ON</button></form>";
    html += "<form action='/heater/off' method='get'><button class='btn btn-off'>Turn OFF</button></form>";
    html += "</div>";

    html += "<div class='thr'>Auto threshold: " + String(TEMP_THRESHOLD, 1) + "&deg;C &nbsp;|&nbsp; hysteresis: " + String(HYSTERESIS, 1) + "&deg;C</div>";
    html += "</div>"; // close .panel

    html += R"(<script>
function switchMode(v){
  if(v==='auto'){
    document.getElementById('mbts').style.display='none';
    window.location='/heater/auto';
  } else {
    document.getElementById('mbts').style.display='flex';
  }
}
function applyData(d){
  var hs=document.getElementById('heater-state');
  hs.textContent=d.heater?'ON':'OFF';
  hs.className=d.heater?'on':'off';
  var isAuto=d.mode==='auto';
  document.getElementById('mode-auto').checked=isAuto;
  document.getElementById('mode-manual').checked=!isAuto;
  document.getElementById('mbts').style.display=isAuto?'none':'flex';
  d.rooms.forEach(function(r,i){
    var card=document.getElementById('card-'+i);
    var tempEl=document.getElementById('temp-'+i);
    var humEl=document.getElementById('hum-'+i);
    var errEl=document.getElementById('err-'+i);
    if(r.online){
      card.className='card';
      tempEl.innerHTML=r.temp.toFixed(1)+'&deg;';
      humEl.textContent=r.humidity.toFixed(1)+'% RH';
      errEl.textContent='';
    } else {
      card.className='card offline';
      tempEl.textContent='--';
      humEl.textContent='offline';
      errEl.textContent=r.error||'';
    }
  });
}
function updatePollStatus(ok){
  var el=document.getElementById('poll-status');
  if(ok){
    var d=new Date();
    var ts=d.getHours().toString().padStart(2,'0')+':'+d.getMinutes().toString().padStart(2,'0')+':'+d.getSeconds().toString().padStart(2,'0');
    el.textContent='last updated '+ts;
    el.className='poll-status fresh';
    setTimeout(function(){el.className='poll-status';},1500);
  } else {
    el.textContent='update failed — retrying\u2026';
  }
}
function poll(){
  fetch('/data').then(function(r){return r.json();}).then(function(d){applyData(d);updatePollStatus(true);}).catch(function(){updatePollStatus(false);}).finally(function(){setTimeout(poll,10000);});
}
setTimeout(poll,10000);
window.onload=function(){
  var sel=document.querySelector('input[name=hmode]:checked');
  if(sel && sel.value==='manual') document.getElementById('mbts').style.display='flex';
};
</script></body></html>)";

    return html;
}

// -------------------------------------------------------
// Route handlers
// -------------------------------------------------------
void handleRoot()
{
    server.send(200, "text/html", buildDashboard());
}

void handleHeaterOn()
{
    heaterMode = FORCE_ON;
    setRelay(true);
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleHeaterOff()
{
    heaterMode = FORCE_OFF;
    setRelay(false);
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleHeaterAuto()
{
    heaterMode = AUTO;
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleData()
{
    StaticJsonDocument<640> doc;
    doc["heater"] = relayState;
    doc["mode"] = (heaterMode == FORCE_ON) ? "force_on" : (heaterMode == FORCE_OFF) ? "force_off"
                                                                                    : "auto";

    JsonArray arr = doc.createNestedArray("rooms");
    for (int i = 0; i < NUM_ROOMS; i++)
    {
        JsonObject r = arr.createNestedObject();
        r["name"] = rooms[i].name;
        r["temp"] = rooms[i].temp;
        r["humidity"] = rooms[i].humidity;
        r["online"] = rooms[i].online;
        r["error"] = rooms[i].error;
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// -------------------------------------------------------
// Setup & loop
// -------------------------------------------------------
void setup()
{
    Serial.begin(115200);

    Serial.print("Starting up...");

    // GSM module init (SIM800L)
    // gsmSerial.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
    // delay(3000);                      // wait for SIM800L to boot
    // gsmCmd("AT");                     // check communication
    // gsmCmd("AT+CMGF=1");              // SMS text mode
    // gsmCmd("AT+CNMI=1,2,0,0,0");     // push new SMS directly to serial as +CMT:
    // gsmCmd("AT+CMGD=1,4");           // delete all stored messages
    // Serial.println("GSM ready");

    pinMode(RELAY_PIN, OUTPUT);
    setRelay(false);

    WiFi.mode(WIFI_STA);
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nController online at http://" + WiFi.localIP().toString());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/heater/on", HTTP_GET, handleHeaterOn);
    server.on("/heater/off", HTTP_GET, handleHeaterOff);
    server.on("/heater/auto", HTTP_GET, handleHeaterAuto);
    server.on("/data", HTTP_GET, handleData);
    server.begin();

    // First poll immediately on boot
    pollNodes();
    applyHeaterLogic();
    lastPoll = millis();
}

void loop()
{
    server.handleClient();
    checkGSM();

    if (millis() - lastPoll >= POLL_INTERVAL_MS)
    {
        lastPoll = millis();
        pollNodes();
        applyHeaterLogic();
    }
}
