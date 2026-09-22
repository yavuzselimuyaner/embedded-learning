# wokwi_iot_sensor: From a Sensor to the Internet

A small IoT node built entirely in the [Wokwi](https://wokwi.com) simulator. The device reads
its sensors, draws them on an OLED, serves them on a web page and publishes them to an MQTT
broker on the internet. It also shows commands that come back from the broker.

This file is written as a tutorial. Each section covers one concept: what it is, where it
appears in the code and why it is done that way. Self-check questions are at the end.

> **Note:** This project uses a classic **ESP32 DevKit** with the **Arduino framework**
> (PlatformIO). The rest of this repo uses ESP32-S3 with ESP-IDF. The MQTT part already uses
> ESP-IDF's own client (`esp-mqtt`).

---

## Contents

0. [The big picture](#0-the-big-picture)
1. [I2C: many devices on two wires](#1-i2c-many-devices-on-two-wires)
2. [Reading a sensor without a library: registers](#2-reading-a-sensor-without-a-library-registers)
3. [DHT22: a sensor with its own protocol](#3-dht22-a-sensor-with-its-own-protocol)
4. [`millis()` instead of `delay()`](#4-millis-instead-of-delay)
5. [Wi-Fi and a web server](#5-wi-fi-and-a-web-server)
6. [MQTT: talking through a broker](#6-mqtt-talking-through-a-broker)
7. [Debugging a blocked network](#7-debugging-a-blocked-network)
8. [WebSocket and TLS](#8-websocket-and-tls)
9. [Two tasks, one variable: race conditions](#9-two-tasks-one-variable-race-conditions)
10. [Self-check](#10-self-check)
11. [Running it](#11-running-it)

---

## 0. The big picture

```
 DHT22 ──(single wire, GPIO15)──┐
                                │
 MPU6050 ──┐                    ▼
           ├─(I2C: 21/22)──── ESP32 ──Wi-Fi──▶ Internet ──▶ MQTT broker ──▶ Browser / phone
 OLED ─────┘                    │                               ▲
                                │                               │
                                └── Web server (port 80)        └── Command "hello" ──▶ OLED
```

| Part | Connection | Role |
|---|---|---|
| DHT22 | GPIO15, 10 kΩ pull-up | Temperature and humidity |
| MPU6050 | I2C, address 0x68 | Acceleration (x, y, z) |
| SSD1306 OLED | I2C, address 0x3C | 128×64 display |

The project was built in three stages:
1. **Local:** read the sensors and show the values on the display.
2. **Same network:** open the ESP32's IP address in a browser to see the data.
3. **Internet:** publish to a broker, then watch from anywhere and send commands back.

---

## 1. I2C: many devices on two wires

### Concept
I2C is a two-wire shared **bus**:
- **SDA:** data
- **SCL:** clock, driven by the ESP32

Every device hangs off the same two wires, and each one has a fixed **address**. The ESP32 is
the **controller** (master) and the sensors are **targets** (slaves). The controller always
starts the conversation.

Think of it as a roll call:

```
ESP32:   "0x68, are you there?"
MPU6050: "Here"                 ← this is an ACK
ESP32:   "0x50, are you there?"
(silence)                       ← this is a NACK: nobody at that address
```

### Why pull-up resistors?
I2C lines are **open-drain**, so devices can only pull a line down to 0 and never drive it up
to 1. The pull-up resistors are what bring the line back to 1. If they are missing or not
connected, the line stays at 0 and nothing can talk. On real hardware this is the most common
cause of I2C problems. Wokwi provides pull-ups automatically, so it never shows up there.

### The I2C scanner as a diagnostic tool
```cpp
for (uint8_t addr = 1; addr < 127; addr++) {
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {   // 0 = device ACKed
    Serial.printf("Device found at 0x%02X\n", addr);
  }
}
```
The scanner probes every address and lists the ones that answer. Its real value is that it
**splits a failure into two layers**:

| Scanner result | Meaning | Where to look |
|---|---|---|
| **Nothing found** | No physical communication | Wiring, pins, pull-ups, power, common ground |
| **Device found** | The wiring is fine | Software: wrong register, sensor still asleep... |

**Why Wokwi helps:** the simulated wiring is perfect. If the code works in Wokwi but not on the
real board, the problem is in the hardware. This separates software faults from hardware faults.

---

## 2. Reading a sensor without a library: registers

### Concept
Inside, a sensor is a set of small numbered cells called **registers**. You write some of them
to configure the sensor and read others to get measurements. The **datasheet** tells you what
each register does. For this sensor it is the MPU-6050 Register Map.

Registers used here:

| Register | Address | Purpose |
|---|---|---|
| `WHO_AM_I` | 0x75 | Identity; always reads 0x68 |
| `PWR_MGMT_1` | 0x6B | Power management; the chip powers up **asleep** |
| `ACCEL_XOUT_H` | 0x3B | Start of the acceleration data (6 bytes: X, Y, Z) |

### Three steps
**1. Ask for its identity** to confirm you are talking to the right chip.
```cpp
readRegisters(0x68, 0x75, &id, 1);   // id == 0x68 means the right chip
```

**2. Wake it up.** Until the sleep bit is cleared, every reading is 0. Only the datasheet tells
you this.
```cpp
writeRegister(0x68, 0x6B, 0x00);
```

**3. Read and convert:**
```cpp
readRegisters(0x68, 0x3B, raw, 6);
ax = (int16_t)(raw[0] << 8 | raw[1]) / 16384.0;
```
- Each axis is **16 bits** split into two bytes, high byte (H) first and low byte (L) second.
  `<< 8` shifts the high byte up so it can be combined with the low one. This high-byte-first
  order is called **big-endian**.
- `(int16_t)`: acceleration can be negative, so the value must be read as **signed**.
- `/ 16384.0`: in the default ±2 g range, 1 g = 16384. This converts the raw count to a
  physical unit.

### How the read works (repeated START)
```cpp
Wire.beginTransmission(addr);
Wire.write(reg);                  // "I want to read from 0x3B"
Wire.endTransmission(false);      // false: keep the bus (no STOP)
Wire.requestFrom(addr, 6);        // now read 6 bytes
```
First you **write** which register to start from, then you **read** without releasing the bus.
The sensor advances its register pointer after every byte, so all 6 bytes arrive in one go.

> Sensor libraries do exactly this under the hood. If you can read a datasheet and write a
> register, you can drive any sensor.

---

## 3. DHT22: a sensor with its own protocol

The DHT22 uses neither I2C nor SPI. It has its own **single-wire protocol** with microsecond
timing, which is why a library handles it here.

Two things to know:
- **Read it at most once every 2 seconds.** Reading faster gives errors or stale values.
- **The data line needs a pull-up.** The 10 kΩ resistor in the circuit provides it.

The first version had the DHT on **GPIO3**. That pin is the serial port's **RX**, so it clashed
with `Serial.begin()`, and the sensor was moved to GPIO15.
**Lesson:** before choosing a pin, check whether it already has another role, such as UART,
boot strapping or flash.

---

## 4. `millis()` instead of `delay()`

### The problem
```cpp
void loop() {
  delay(2000);        // the CPU does NOTHING for 2 seconds
  readSensor();
}
```
This breaks once a web server is added. The server only answers requests while
`server.handleClient()` is running, so during a `delay()` the browser just waits.

### The fix: check the clock instead of waiting
```cpp
void loop() {
  server.handleClient();                    // every pass: answer requests

  static unsigned long last = 0;
  if (millis() - last < 500) return;        // 500 ms not up yet, leave
  last = millis();

  readSensor();                             // runs every 500 ms
}
```
`millis()` returns the milliseconds since boot. `loop()` spins thousands of times per second,
but each job only runs when it is due. Different jobs can run at different rates: acceleration
every 500 ms, the DHT every 2 s and MQTT every 5 s.

> `millis() - last` is safe across overflow. The counter wraps about every 49 days, and unsigned
> subtraction still gives the right answer.

**Later:** in FreeRTOS the same problem is solved by giving each job its own **task**.

---

## 5. Wi-Fi and a web server

### Client and server
```
Browser (client) ──"GET /"──▶ ESP32 (server, port 80)
Browser          ◀──HTML───── ESP32
```
- **IP address:** the device's address on the network (e.g. `10.13.37.2`).
- **Port:** a numbered "door" on that device. The web uses 80 (HTTP) and 443 (HTTPS).
- **Path:** `/` returns the page and `/data` returns JSON.

```cpp
server.on("/", handleRoot);       // when "/" is requested, run handleRoot()
server.on("/data", handleData);   // when "/data" is requested, return JSON
```

HTML is for people and JSON is for **programs**. Other software can fetch `/data` and parse it
easily.

### Port forwarding in Wokwi
The ESP32's IP belongs to Wokwi's virtual network, which your computer cannot see. This part of
`wokwi.toml` builds a bridge:
```toml
[[net.forward]]
from = "localhost:8180"   # a port on your computer
to = "target:80"          # port 80 on the simulated ESP32
```
> Wokwi reads `wokwi.toml` **only when the simulation starts**. Restart the simulator after
> editing it.

### The limitation
This only works **on the same network**. If the ESP32 is at home and you are elsewhere, you
cannot reach it, because the home router blocks incoming connections. With 100 devices you would
also have to poll each one. MQTT solves both problems.

---

## 6. MQTT: talking through a broker

### Concept
A middleman server, the **broker**, sits in between:
```
ESP32 ──publish──▶  BROKER  ──forward──▶  Every subscriber
                 (on the internet)
```
- The ESP32 does not wait to be asked. It **pushes** its data.
- The broker forwards each message to everyone listening on that topic.
- The ESP32 does not know who is listening, and you do not need to know where the ESP32 is.
  Both sides only know the broker.

**Why this works:** networks allow outgoing connections and block incoming ones. Here the ESP32
and you both connect **outward** to the broker, which acts as a meeting point.

> Analogy: a group chat. The ESP32 posts to the group and the chat server (the broker) delivers
> the message to every member.

### Key concepts

| Concept | Meaning | In this project |
|---|---|---|
| **Topic** | The "channel" a message goes to; hierarchical with `/` | `yavuz-iot-sensor/esp32/data` |
| **Publish** | Send a message to a topic | The ESP32 publishes every 5 s |
| **Subscribe** | Listen to a topic | The ESP32 listens on `.../command` |
| **Wildcard `#`** | "Everything below this" | `yavuz-iot-sensor/esp32/#` matches all three topics |
| **Wildcard `+`** | Exactly one level | `+/esp32/data` |
| **Retained** | The broker keeps the last message and sends it to new subscribers at once | `status` |
| **Last Will (LWT)** | A message left with the broker: "publish this if I vanish" | `offline` |
| **Keepalive** | Silence longer than this means the client is gone | 15 s |
| **QoS** | Delivery guarantee: 0 = at most once, 1 = at least once, 2 = exactly once | data 0, status 1 |

### Topics used

| Topic | Direction | Payload |
|---|---|---|
| `.../data` | ESP32 → broker | `{"temperature":24.0,"humidity":40.0,"ax":0.00,"ay":0.00,"az":1.00}` |
| `.../status` | ESP32 → broker | `online` / `offline` (retained) |
| `.../command` | broker → ESP32 | Short text, shown on the OLED as `> text` |

### Presence tracking: retained plus Last Will
1. On connect, the ESP32 leaves a will with the broker: *"if I drop, publish `offline` to
   `status`."*
2. Once connected, it publishes `online` itself as a retained message.
3. If power is cut, the ESP32 cannot say goodbye. After 15 s of silence the broker publishes the
   will on its behalf.
4. Because the status is retained, anyone who subscribes later sees the device's state
   immediately.

### What the library does
`esp-mqtt` **reconnects on its own** when the link drops. Events arrive in a callback:
```cpp
void onMqttEvent(..., int32_t eventId, void *eventData) {
  switch (eventId) {
    case MQTT_EVENT_CONNECTED:    // publish status, subscribe to commands
    case MQTT_EVENT_DISCONNECTED: // the library will retry
    case MQTT_EVENT_DATA:         // a message arrived on a subscribed topic
    case MQTT_EVENT_ERROR:        // something failed
  }
}
```
This is **event-driven** programming. Instead of repeatedly asking whether a message has
arrived, your code is called when one does.

---

## 7. Debugging a blocked network

This is the most instructive part of the project, because real work goes exactly like this.

### Symptom
```
MQTT connect failed (state -2)
```
`-2` means **the TCP connection itself failed**. The client never even got to say hello to the
broker.

### Possible causes
- DNS cannot resolve the broker's name
- The simulation has no internet access
- The broker is down
- The network blocks that port

### Measure, don't guess
A boot-time **diagnostic** was added to the firmware:
```
DNS: test.mosquitto.org -> 54.36.178.49     ← DNS works
  Port 80:   OPEN                           ← internet works
  Port 1883: CLOSED                         ← MQTT port blocked
  Port 8883: CLOSED                         ← MQTT-over-TLS port blocked too
```
The same test run from the host PC (`Test-NetConnection`) gave **the same result**. Wokwi's VS
Code extension routes simulated traffic through the host, so the ESP32 hits the same firewall as
the PC.

**Conclusion:** the code was fine. The network only allowed web ports (80 and 443), which is
common on school, dorm and corporate networks.

### The general lesson
When debugging, **split the problem into layers and test each one on its own**:
```
DNS works? → Internet works? → Port open? → Protocol right? → Application
```
Each layer depends on the one before it. The I2C scanner applies the same idea.

---

## 8. WebSocket and TLS

### WebSocket: MQTT disguised as web traffic
With only port 443 open, the MQTT packets were wrapped in a **WebSocket** and sent over 443.
The messages are the same and only the door is different. Firewalls see ordinary web traffic.

| URI scheme | Meaning | Port |
|---|---|---|
| `mqtt://` | Plain MQTT | 1883 |
| `mqtts://` | MQTT over TLS | 8883 |
| `ws://` | MQTT over WebSocket | 80 |
| `wss://` | MQTT over secure WebSocket | 443 ← **used here** |

This is also why the library changed. PubSubClient cannot do WebSocket, but `esp-mqtt` can,
and it already ships inside the Arduino core.

### TLS: encryption and identity
TLS does two jobs:
1. **Encryption:** nobody in between can read the messages.
2. **Authentication:** it proves the server really is who it claims to be.

The second job relies on a **certificate chain**:
```
public.cloud.shiftr.io  (server certificate)
        ↑ signed by
Let's Encrypt YR1       (intermediate)
        ↑ signed by
ISRG Root X1            (ROOT ← the only thing the ESP32 trusts)
```
The ESP32 is given only the **root certificate** (`include/ca_cert.h`). The server sends the
rest of the chain during the handshake, and the ESP32 checks that it leads back to that root.

### Why `setInsecure()` is bad
An earlier version used `setInsecure()`. That keeps the traffic encrypted but **skips identity
verification**, so an attacker could pose as the broker and the ESP32 would not notice. This is a
**man-in-the-middle** attack. It is acceptable for a quick test and never for a product.

> Root certificates expire too. ISRG Root X1 expires in 2035, which is one reason real devices
> need a way to update certificates over the air (OTA).

---

## 9. Two tasks, one variable: race conditions

### The situation
`esp-mqtt` runs in its own **FreeRTOS task**, and `loop()` runs in another. They can run **at
the same time**:
```
MQTT task:  command arrives → writing "hello" into lastCommand...
loop task:  ...reads lastCommand at that exact moment to draw it
```
If the read lands halfway through the write, the screen shows a partial or garbled string. This
is a **race condition**. It happens rarely and is hard to reproduce, which makes it one of the
hardest kinds of bug to find.

### The fix: a critical section
```cpp
portENTER_CRITICAL(&commandLock);
memcpy(lastCommand, event->data, n);   // nobody can interrupt this
portEXIT_CRITICAL(&commandLock);
```
The reader takes the same lock, copies the string and releases the lock right away:
```cpp
portENTER_CRITICAL(&commandLock);
strcpy(command, lastCommand);
portEXIT_CRITICAL(&commandLock);
display.printf("> %s", command);       // slow work stays OUTSIDE the lock
```
**Rule:** keep critical sections as short as possible, because interrupts wait while one is held.

### What `volatile` is for
```cpp
volatile bool mqttConnected = false;
```
This tells the compiler that something else may change the variable, so it must re-read it from
memory every time. That is enough for a single `bool`. It is **not** enough for multi-byte data
like a string, which needs a lock.

---

## 10. Self-check

The answers are in the sections above.

1. The I2C scanner finds nothing. Where do you look first, and why not the code?
2. What do you read from register `0x3B` if you never woke the MPU6050?
3. Why is `raw[0] << 8 | raw[1]` cast to `(int16_t)`?
4. Why does a `delay(2000)` in `loop()` make the web page slow?
5. Why can't you see the data from another city with the web server approach, and how does MQTT
   fix that?
6. What problem do retained messages and Last Will solve together?
7. You get `state -2`. What steps would you take to find the cause?
8. Does `setInsecure()` turn off encryption? What does it turn off?
9. Why give the ESP32 the root certificate rather than the server's own certificate?
10. Two tasks share a string. Why is `volatile` not enough?

---

## 11. Running it

Requirements: VS Code with the PlatformIO and Wokwi extensions.

```
pio run                        # build
F1 → Wokwi: Start Simulator    # start the simulation
```

| What | Where |
|---|---|
| Web page | http://localhost:8180 |
| JSON data | http://localhost:8180/data |
| Live broker view | https://public.cloud.shiftr.io |

**To send a command**, use any MQTT client (e.g. HiveMQ Web Client): host
`public.cloud.shiftr.io`, port `443`, SSL on, username and password `public` / `public`.
Publish to `yavuz-iot-sensor/esp32/command`.

> **Warning:** the broker is public. Anyone who knows the topic can read the data or send
> commands. That is fine for a demo. A real device would use an authenticated broker where each
> device can access only its own topics.

### Files

| File | Contents |
|---|---|
| `src/main.cpp` | The whole firmware |
| `include/ca_cert.h` | ISRG Root X1 root certificate |
| `diagram.json` | Wokwi circuit |
| `wokwi.toml` | Firmware path and port forwarding |
| `platformio.ini` | Board, framework, libraries |
