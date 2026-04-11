import os
import random
import threading
import time
from flask import Flask, jsonify, request

app = Flask(__name__)

ROOM_NAME = os.environ.get("ROOM_NAME", "Unknown")

# Simulated sensor state
state = {
    "temp": float(os.environ.get("INIT_TEMP", "21.0")),
    "humidity": float(os.environ.get("INIT_HUMIDITY", "50.0")),
}
lock = threading.Lock()

# Drift mode: if enabled, temp/humidity fluctuate slightly every few seconds
DRIFT_ENABLED = os.environ.get("DRIFT", "true").lower() == "true"


def drift_loop():
    """Slowly drift temp/humidity by small random amounts to simulate real sensors."""
    while True:
        time.sleep(5)
        with lock:
            state["temp"] += random.uniform(-0.15, 0.15)
            state["humidity"] += random.uniform(-0.3, 0.3)
            state["humidity"] = max(0.0, min(100.0, state["humidity"]))


if DRIFT_ENABLED:
    t = threading.Thread(target=drift_loop, daemon=True)
    t.start()


@app.route("/data", methods=["GET"])
def get_data():
    with lock:
        return jsonify({
            "room": ROOM_NAME,
            "temp": round(state["temp"], 1),
            "humidity": round(state["humidity"], 1),
        })


@app.route("/ping", methods=["GET"])
def ping():
    return "OK", 200


@app.route("/set", methods=["POST"])
def set_values():
    """Test helper: POST {"temp": 18.0, "humidity": 60} to override sensor values."""
    data = request.get_json(silent=True) or {}
    with lock:
        if "temp" in data:
            state["temp"] = float(data["temp"])
        if "humidity" in data:
            state["humidity"] = float(data["humidity"])
    return jsonify({"ok": True, "state": state})


if __name__ == "__main__":
    port = int(os.environ.get("PORT", "80"))
    app.run(host="0.0.0.0", port=port)
