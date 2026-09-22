# wokwi_iot_sensor

A small end-to-end IoT node, built and tested entirely in the
[Wokwi](https://wokwi.com) simulator: read sensors, show them on an OLED,
serve them over HTTP, and publish them to an MQTT broker. The broker link is two-way, so a
command sent from anywhere on the internet appears on the device's screen.

> This project uses a classic **ESP32 DevKit** with the **Arduino framework** (PlatformIO),
> not the ESP32-S3 / ESP-IDF setup used by the rest of this repo. The MQTT part already
> uses ESP-IDF's own client (`esp-mqtt`), so moving it to ESP-IDF later is straightforward.

## Circuit

| Part | Bus / pin | Notes |
|---|---|---|
| DHT22 (temperature, humidity) | GPIO15, single-wire | 10 kΩ pull-up on the data line |
| SSD1306 128×64 OLED | I2C 0x3C, SDA 21 / SCL 22 | |
| MPU6050 (accelerometer) | I2C 0x68, same bus | Read by raw register access, no library |

The wiring is in [`diagram.json`](diagram.json).

## What it does, in three stages

1. **Local:** read the DHT22 every 2 s and the MPU6050 every 500 ms, then draw both on the OLED.
2. **HTTP:** join Wi-Fi and run a web server. `/` is an auto-refreshing page and `/veri` returns JSON.
3. **MQTT:** publish readings every 5 s and subscribe to a command topic.

| Topic | Direction | Payload |
|---|---|---|
| `yavuz-sensordenemesi/esp32/veri` | device → broker | `{"sicaklik":24.0,"nem":40.0,"ax":0.00,"ay":0.00,"az":1.00}` |
| `yavuz-sensordenemesi/esp32/durum` | device → broker | `cevrimici` / `cevrimdisi` (retained, and set by Last Will on drop) |
| `yavuz-sensordenemesi/esp32/komut` | broker → device | any short text, shown on the OLED |

## Things worth noting

**I2C scan and raw register access.** On boot the firmware scans addresses 1–126 and prints
every device that ACKs. The MPU6050 is then driven straight from the datasheet: it checks
`WHO_AM_I` (0x75 = 0x68), wakes the chip by clearing `PWR_MGMT_1` (0x6B, it powers up asleep),
and burst-reads 6 bytes from `ACCEL_XOUT_H` (0x3B). At ±2 g, 1 g is 16384 LSB.

**No blocking in `loop()`.** The web server only answers while `server.handleClient()` runs,
so the original `delay()` calls were replaced by `millis()` checks.

**Debugging a blocked network.** MQTT failed with `-2`, meaning the TCP connect itself failed. A
boot-time port probe on the device, plus `Test-NetConnection` on the host, both showed the
same thing: only ports 80 and 443 were open, and 1883, 8883 and 8081 were blocked. Wokwi's VS Code
gateway routes simulated traffic through the host, so the device inherits the host network's
firewall. The fix was **MQTT over secure WebSocket on port 443**
(`wss://public.cloud.shiftr.io`). PubSubClient cannot do WebSocket, so the code switched to
ESP-IDF's `esp-mqtt`, which is bundled with the Arduino core.

**TLS with a pinned root CA.** The broker's chain ends at ISRG Root X1. That root is embedded in
[`include/ca_sertifika.h`](include/ca_sertifika.h), so the device verifies the server instead of
using `setInsecure()`.

**Shared state across tasks.** `esp-mqtt` delivers events from its own FreeRTOS task while
`loop()` runs in another. The last received command is copied under a `portMUX` critical
section so the OLED never reads a half-written string.

## Run it

Requires VS Code with the PlatformIO and Wokwi extensions.

```
pio run                      # build; wokwi.toml points at .pio/build/esp32doit-devkit-v1/
F1 → Wokwi: Start Simulator
```

- Web page: http://localhost:8180, forwarded to the device's port 80 by `wokwi.toml`.
  Restart the simulator after editing `wokwi.toml`.
- Live broker view: https://public.cloud.shiftr.io
- To send a command, use any MQTT client with host `public.cloud.shiftr.io`, port 443, TLS,
  WebSocket and user/password `public`/`public`. Publish to the `komut` topic.

The broker is public. Anyone who knows the topic can read the data or send commands, which is
fine for a demo but not for a real device.
