ESP32 MQTT mutual-TLS test
=========================

This workspace contains:

- `mqtt_tls_check.py` — desktop TLS verifier that suggests the broker's TLS name.
- `esp32_mqtt_tls_test.ino` — Arduino sketch that performs a mutual-TLS MQTT connect.

Quick (easiest) test approach — Arduino IDE
-----------------------------------------

1. Install Arduino IDE (or use the Arduino extension in VS Code).
2. Install ESP32 board support: Tools → Board → Boards Manager → search "esp32" → install.
3. Install library: Sketch → Include Library → Manage Libraries → search "PubSubClient" → install.
4. Open [esp32_mqtt_tls_test.ino](esp32_mqtt_tls_test.ino#L1-L200) in the IDE.
5. Replace Wi‑Fi placeholders and paste PEM contents (root CA, client cert, client key) into the constants near the top.
6. If the broker's certificate CN/SAN differs from the connect host, set `tls_server_name` to the suggested name from `mqtt_tls_check.py` (for example `emqx.emqx.svc.cluster.local`).
7. Select board (e.g., "ESP32 Dev Module") and the COM port, then Upload.
8. Open Serial Monitor at `115200` baud and watch for "MQTT connected" and "Published heartbeat" messages.

PlatformIO (optional)
----------------------

1. Create a PlatformIO project for board `esp32dev` (Framework Arduino).
2. Copy `esp32_mqtt_tls_test.ino` to `src/main.ino` inside the project.
3. Add `lib_deps = knolleary/PubSubClient@^2.8` to `platformio.ini`.
4. Build and upload:

```bash
pio run -e esp32dev -t upload
pio device monitor --baud 115200
```

Troubleshooting
---------------
- If you see a TLS/hostname mismatch, run `python mqtt_tls_check.py` on your PC to get the certificate SAN/CN and use that value as `tls_server_name` in the sketch.
- Ensure port 8883 is reachable from your network and the broker accepts client-certificate authentication.
- If upload fails, verify USB driver and COM port selection.

Security note
-------------
Embedding private keys in source is convenient for testing but insecure for production. For longer-term use, store certs/keys in SPIFFS/LittleFS or use a secure element.
