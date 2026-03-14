import os
import time
import threading
from datetime import datetime
from flask import Flask, jsonify, request, redirect
import requests as http_client

app = Flask(__name__)

# ---------------------------------------------------------------------------
# Configuration (mirrors controller/include/config.h)
# ---------------------------------------------------------------------------
NODES = [
    {"name": "Bedroom",     "url": os.environ.get("NODE1_URL", "http://node-bedroom:80")},
    {"name": "Kitchen",     "url": os.environ.get("NODE2_URL", "http://node-kitchen:80")},
    {"name": "Living Room", "url": os.environ.get("NODE3_URL", "http://node-living:80")},
]

TEMP_THRESHOLD = float(os.environ.get("TEMP_THRESHOLD", "20.0"))
HYSTERESIS     = float(os.environ.get("HYSTERESIS", "0.5"))
POLL_INTERVAL  = int(os.environ.get("POLL_INTERVAL", "10"))
ADMIN_PHONE    = os.environ.get("ADMIN_PHONE", "+1234567890")
RELAY_PIN      = 26  # informational only

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------
rooms = [{"name": n["name"], "url": n["url"], "temp": 0.0, "humidity": 0.0, "online": False}
         for n in NODES]

heater_mode  = "auto"      # "auto" | "force_on" | "force_off"
relay_state  = False
prev_below   = False
sms_outbox   = []          # list of {"to", "text", "timestamp"}
lock         = threading.Lock()


def set_relay(on: bool):
    global relay_state
    relay_state = on


def send_sms(phone: str, text: str):
    entry = {"to": phone, "text": text, "timestamp": datetime.now().isoformat()}
    sms_outbox.append(entry)
    print(f"[SMS → {phone}] {text}")


# ---------------------------------------------------------------------------
# Polling loop (background thread)
# ---------------------------------------------------------------------------
def poll_nodes():
    global prev_below
    for room in rooms:
        try:
            r = http_client.get(f"{room['url']}/data", timeout=3)
            r.raise_for_status()
            data = r.json()
            room["temp"]     = data.get("temp", 0.0)
            room["humidity"] = data.get("humidity", 0.0)
            room["online"]   = True
        except Exception:
            room["online"] = False


def apply_heater_logic():
    global prev_below

    if heater_mode == "force_on":
        set_relay(True)
        return
    if heater_mode == "force_off":
        set_relay(False)
        return

    any_below  = False
    all_above  = True
    for r in rooms:
        if not r["online"]:
            continue
        if r["temp"] < TEMP_THRESHOLD:
            any_below = True
        if r["temp"] <= TEMP_THRESHOLD + HYSTERESIS:
            all_above = False

    if not relay_state and any_below:
        set_relay(True)
    if relay_state and all_above:
        set_relay(False)

    # SMS alerts on state transitions
    if not prev_below and any_below:
        prev_below = True
        msg = f"ALERT: Temp dropped below {TEMP_THRESHOLD:.1f}C!\n"
        for r in rooms:
            if r["online"]:
                msg += f"{r['name']}: {r['temp']:.1f}C\n"
        msg += "Heater: ON (Auto)"
        send_sms(ADMIN_PHONE, msg)
    elif prev_below and all_above:
        prev_below = False
        msg = f"INFO: Temp restored above {TEMP_THRESHOLD + HYSTERESIS:.1f}C.\n"
        for r in rooms:
            if r["online"]:
                msg += f"{r['name']}: {r['temp']:.1f}C\n"
        msg += "Heater: OFF (Auto)"
        send_sms(ADMIN_PHONE, msg)


def poll_loop():
    while True:
        with lock:
            poll_nodes()
            apply_heater_logic()
        time.sleep(POLL_INTERVAL)


# ---------------------------------------------------------------------------
# SMS command processor (mirrors GSM processSMS)
# ---------------------------------------------------------------------------
def build_status_sms():
    msg = "Smart Dacha Status:\n"
    for r in rooms:
        msg += f"{r['name']}: "
        if r["online"]:
            msg += f"{r['temp']:.1f}C, {r['humidity']:.1f}%RH\n"
        else:
            msg += "OFFLINE\n"
    msg += f"Heater: {'ON' if relay_state else 'OFF'} ("
    msg += {"auto": "Auto", "force_on": "Force ON", "force_off": "Force OFF"}[heater_mode]
    msg += ")"
    return msg


def process_sms(sender: str, text: str):
    global heater_mode
    if sender != ADMIN_PHONE:
        return

    text = text.strip().upper()
    if text == "STATUS":
        send_sms(ADMIN_PHONE, build_status_sms())
    elif text == "HEATER ON":
        heater_mode = "force_on"
        set_relay(True)
        send_sms(ADMIN_PHONE, "Heater forced ON.")
    elif text == "HEATER OFF":
        heater_mode = "force_off"
        set_relay(False)
        send_sms(ADMIN_PHONE, "Heater forced OFF.")
    elif text == "HEATER AUTO":
        heater_mode = "auto"
        send_sms(ADMIN_PHONE, "Heater set to AUTO mode.")
    else:
        send_sms(ADMIN_PHONE, "Unknown cmd. Valid:\nSTATUS\nHEATER ON\nHEATER OFF\nHEATER AUTO")


# ---------------------------------------------------------------------------
# Dashboard HTML (mirrors buildDashboard in firmware)
# ---------------------------------------------------------------------------
def build_dashboard():
    is_auto = heater_mode == "auto"

    cards = ""
    for r in rooms:
        cls = "card offline" if not r["online"] else "card"
        if r["online"]:
            cards += (f"<div class='{cls}'><div class='room'>{r['name']}</div>"
                      f"<div class='temp'>{r['temp']:.1f}&deg;</div>"
                      f"<div class='hum'>{r['humidity']:.1f}% RH</div></div>")
        else:
            cards += (f"<div class='{cls}'><div class='room'>{r['name']}</div>"
                      f"<div class='temp'>--</div><div class='hum'>offline</div></div>")

    auto_chk = " checked" if is_auto else ""
    manual_chk = "" if is_auto else " checked"
    btns_display = "none" if is_auto else "flex"

    return f"""<!DOCTYPE html><html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <meta http-equiv='refresh' content='10'>
  <title>Smart Dacha</title>
  <style>
    body{{font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:20px;margin:0}}
    h1{{color:#e94560;margin-bottom:4px}}
    .subtitle{{color:#6c6c8a;font-size:.85em;margin-bottom:28px}}
    .grid{{display:flex;flex-wrap:wrap;justify-content:center;gap:16px;margin-bottom:28px}}
    .card{{background:#16213e;border-radius:14px;padding:22px 28px;min-width:140px}}
    .card.offline{{opacity:.35}}
    .room{{font-size:.8em;color:#7a7aaa;margin-bottom:6px;text-transform:uppercase;letter-spacing:.05em}}
    .temp{{font-size:2.6em;font-weight:700;color:#e94560;line-height:1}}
    .hum{{font-size:1em;color:#00b4d8;margin-top:6px}}
    .panel{{background:#16213e;border-radius:14px;padding:20px;max-width:320px;margin:0 auto}}
    .panel h2{{margin:0 0 14px;font-size:1em;text-transform:uppercase;letter-spacing:.05em;color:#7a7aaa}}
    .state{{font-size:1.3em;font-weight:600;margin-bottom:16px}}
    .on{{color:#2ecc71}} .off{{color:#e74c3c}}
    .mode-switch{{display:flex;justify-content:center;gap:12px;margin-bottom:16px}}
    .radio-lbl{{display:flex;align-items:center;gap:8px;cursor:pointer;font-size:.95em;background:#0f3460;padding:9px 20px;border-radius:8px;transition:background .2s}}
    .radio-lbl:hover{{background:#1a4a80}}
    .radio-lbl input{{accent-color:#e94560;width:16px;height:16px;cursor:pointer}}
    .btns{{display:flex;gap:8px;justify-content:center;flex-wrap:wrap}}
    .btn{{border:none;border-radius:8px;padding:9px 28px;font-size:.95em;cursor:pointer;font-weight:600}}
    .btn-on {{background:#2ecc71;color:#000}}
    .btn-off{{background:#e74c3c;color:#fff}}
    .thr{{font-size:.8em;color:#555580;margin-top:14px}}
  </style>
</head>
<body>
<h1>Smart Dacha</h1>
<div class='subtitle'>EMULATOR &mdash; auto-refreshes every 10 s</div>
<div class='grid'>{cards}</div>
<div class='panel'><h2>Gas Heater</h2>
<div class='state'>State: <span class='{"on" if relay_state else "off"}'>{"ON" if relay_state else "OFF"}</span></div>
<div class='mode-switch'>
  <label class='radio-lbl'><input type='radio' name='hmode' value='auto' onchange="switchMode(this.value)"{auto_chk}> Auto</label>
  <label class='radio-lbl'><input type='radio' name='hmode' value='manual' onchange="switchMode(this.value)"{manual_chk}> Manual</label>
</div>
<div id='mbts' class='btns' style='display:{btns_display}'>
  <form action='/heater/on' method='get'><button class='btn btn-on'>Turn ON</button></form>
  <form action='/heater/off' method='get'><button class='btn btn-off'>Turn OFF</button></form>
</div>
<div class='thr'>Auto threshold: {TEMP_THRESHOLD:.1f}&deg;C &nbsp;|&nbsp; hysteresis: {HYSTERESIS:.1f}&deg;C</div>
</div>
<script>
function switchMode(v){{
  if(v==='auto'){{document.getElementById('mbts').style.display='none';window.location='/heater/auto';}}
  else{{document.getElementById('mbts').style.display='flex';}}
}}
window.onload=function(){{
  var sel=document.querySelector('input[name=hmode]:checked');
  if(sel && sel.value==='manual') document.getElementById('mbts').style.display='flex';
}};
</script></body></html>"""


# ---------------------------------------------------------------------------
# Routes
# ---------------------------------------------------------------------------
@app.route("/")
def dashboard():
    with lock:
        html = build_dashboard()
    return html, 200, {"Content-Type": "text/html; charset=utf-8"}


@app.route("/heater/on")
def heater_on():
    global heater_mode
    with lock:
        heater_mode = "force_on"
        set_relay(True)
    return redirect("/")


@app.route("/heater/off")
def heater_off():
    global heater_mode
    with lock:
        heater_mode = "force_off"
        set_relay(False)
    return redirect("/")


@app.route("/heater/auto")
def heater_auto():
    global heater_mode
    with lock:
        heater_mode = "auto"
    return redirect("/")


@app.route("/api/status")
def api_status():
    with lock:
        data = {
            "heater": relay_state,
            "mode":   heater_mode,
            "rooms":  [{"name": r["name"], "temp": r["temp"],
                        "humidity": r["humidity"], "online": r["online"]}
                       for r in rooms],
        }
    return jsonify(data)


# --- SMS mock endpoints ---

@app.route("/sms", methods=["POST"])
def receive_sms():
    """Simulate an incoming SMS.  POST {"from": "+1234567890", "text": "STATUS"}"""
    body = request.get_json(silent=True) or {}
    sender = body.get("from", "")
    text   = body.get("text", "")
    if not sender or not text:
        return jsonify({"error": "provide 'from' and 'text'"}), 400
    with lock:
        process_sms(sender, text)
    return jsonify({"ok": True})


@app.route("/sms/outbox")
def get_sms_outbox():
    """View all SMS messages 'sent' by the controller (alerts + command replies)."""
    with lock:
        return jsonify(sms_outbox)


@app.route("/sms/outbox/clear", methods=["POST"])
def sms_outbox_clear():
    with lock:
        sms_outbox.clear()
    return jsonify({"ok": True})


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    poller = threading.Thread(target=poll_loop, daemon=True)
    poller.start()
    port = int(os.environ.get("PORT", "80"))
    app.run(host="0.0.0.0", port=port)
