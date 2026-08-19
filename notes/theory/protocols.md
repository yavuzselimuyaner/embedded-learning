# Serial protocols: UART, I2C, SPI

The three buses every embedded job asks about. They all move bytes between chips; they
differ in how many wires, who controls timing, and how devices are addressed.

---

## The comparison

| | UART | I2C | SPI |
|---|---|---|---|
| Wires (2 devices) | 2 (TX, RX) | 2 (SDA, SCL) | 4 (MOSI, MISO, SCK, CS) |
| Wires (N devices) | not designed for it | still 2 | 3 + one CS per device |
| Clock | none — both sides agree a rate | shared, driven by master | shared, driven by master |
| Type | asynchronous | synchronous | synchronous |
| Addressing | none (point to point) | 7-bit address on the bus | one chip-select line per device |
| Duplex | full | half | full |
| Acknowledgement | none | yes, per byte | none |
| Typical speed | 9.6 k – 921.6 k baud | 100 k / 400 k / 1 M | 1 – 80 MHz |
| Distance | metres (more with RS-485/RS-232) | centimetres, same board | centimetres |
| Pin cost | low | lowest for many devices | highest |
| Complexity | lowest | medium | low protocol, more wires |

**Rules of thumb**

- Talking to a PC, a modem, a GPS module, a debug console → **UART**
- Several low-speed sensors sharing few pins → **I2C**
- One fast device: display, SD card, flash, ADC → **SPI**

---

## UART

**Universal Asynchronous Receiver/Transmitter.** Point to point, no clock line. Both
sides are pre-configured to the same bit rate and sample the line on their own clock.

### The frame

```
 idle   start   d0 d1 d2 d3 d4 d5 d6 d7   parity  stop   idle
──────┐       ┌──┬──┬──┬──┬──┬──┬──┬──┬─────────┐      ┌──────
      └───────┘  │  │  │  │  │  │  │  │         └──────┘
```

- Line idles **high**
- **Start bit**: one bit time low — this is what wakes the receiver
- **Data**: 5–9 bits, **least significant bit first**
- **Parity**: optional (none / even / odd)
- **Stop**: 1, 1.5 or 2 bit times high

"115200 8N1" means 115200 bits/second, 8 data bits, No parity, 1 stop bit. Because of the
start and stop bits, one byte actually costs 10 bit times — so 115200 baud carries about
11.5 kB/s, not 14.4.

### How the receiver stays in sync

There is no shared clock, so the receiver oversamples (typically 16×) and looks for the
falling edge of the start bit, then samples each following bit in the middle of its
window. That works because it only has to stay accurate for ~10 bits before the next
start bit resynchronises it. Total clock mismatch above roughly 2–3 % corrupts data.

### Failure modes

| Symptom | Likely cause |
|---|---|
| Garbage characters | baud rate mismatch on the two sides |
| Nothing at all | TX/RX not crossed, or no common ground |
| Occasional corrupted bytes | clock drift, noise, or missing flow control |
| First byte lost | receiver was still initialising |

**TX goes to RX.** Both sides also need a **shared ground** — a UART link with no common
ground reference does not work, and this is a classic beginner failure.

### Variants

- **TTL UART** — the raw 3.3 V / 5 V logic-level signals off the MCU pin
- **RS-232** — same framing, ±12 V levels, needs a level shifter (MAX232)
- **RS-485** — differential pair, multi-drop, hundreds of metres, industrial
- **Modbus RTU** runs on top of RS-485

Flow control (RTS/CTS) exists but is often unused on short links.

---

## I2C

**Inter-Integrated Circuit.** Two wires shared by many devices: **SDA** (data) and
**SCL** (clock). A master drives the clock and initiates all transfers.

### Open-drain and why pull-ups are mandatory

Every device on an I2C bus can only pull the line **low**. Nobody drives it high. The
line returns high through an external **pull-up resistor** (typically 4.7 kΩ).

This is called **open-drain** (or open-collector). The reason: if two devices could drive
the line actively, one pulling high while another pulls low would short the supply. With
open-drain, simultaneous drivers just produce a low — safe. This also makes the bus a
natural wired-AND, which is exactly how acknowledgement and arbitration work.

Consequence: **no pull-ups, no I2C.** Most sensor breakout boards include them, which is
why a single module often works with no extra parts. Put five modules on the same bus and
the parallel pull-ups become too strong — a real problem in practice.

### A transaction

```
START | addr(7) R/W | ACK | data | ACK | data | ACK | STOP
```

- **START**: SDA falls while SCL is high (the only time SDA may change while SCL is high)
- **Address**: 7 bits, then 1 bit for direction (0 = write, 1 = read)
- **ACK**: after every byte the receiver pulls SDA low for one clock. No ACK (NACK) means
  "nobody is there" or "I am done"
- **STOP**: SDA rises while SCL is high

**Repeated START** is used to switch direction without releasing the bus — the standard
way to read a register: write the register address, repeated START, then read. Releasing
the bus in between would let another master interleave.

### The `addr << 1` confusion

Addresses are 7 bits, but the address *byte* on the wire is `(addr << 1) | rw`. Datasheets
sometimes list the 7-bit address (`0x68`) and sometimes the 8-bit write address (`0xD0`).
Same device. If a scanner finds a device at half the address the datasheet claims, this is
why.

### Clock stretching

A slave that needs more time holds SCL low. The master must wait. Not all master hardware
supports this properly — a known source of obscure bugs.

### Speeds

| Mode | Rate |
|---|---|
| Standard | 100 kHz |
| Fast | 400 kHz |
| Fast Plus | 1 MHz |
| High Speed | 3.4 MHz |

Higher speeds need lower pull-up values, because the rise time is set by the pull-up
resistance and the bus capacitance. Long wires add capacitance and round off the edges —
this is why I2C is a same-board bus.

### Failure modes

| Symptom | Likely cause |
|---|---|
| Every address NACKs | no pull-ups, or SDA/SCL swapped |
| Device found at half the expected address | 7-bit vs 8-bit address confusion |
| Works at 100 kHz, fails at 400 kHz | pull-ups too weak, or wires too long |
| Bus stuck low forever | a slave mid-transfer when the master reset |
| Intermittent errors with many devices | parallel pull-ups on several breakout boards |

### The first thing to write

A **bus scanner**: loop addresses 0x00–0x7F, send the address byte, see who ACKs. It
answers "is the wiring right and is the device alive" in one step, before any driver code
exists. Write it before anything else.

---

## SPI

**Serial Peripheral Interface.** Four wires, fastest of the three, no addressing and no
acknowledgement — the master just clocks bits and the slave clocks bits back at the same
time.

### The wires

| Signal | Meaning |
|---|---|
| **SCK** | clock, from master |
| **MOSI** | Master Out, Slave In |
| **MISO** | Master In, Slave Out |
| **CS** / SS | Chip Select, active **low**, one per slave |

SPI is genuinely full duplex: a byte goes out while a byte comes in. To read, you clock
out a dummy byte; the data you want arrives on MISO during the same eight clocks. Two
shift registers connected in a ring.

Adding devices means adding CS lines, one GPIO each. That is SPI's real cost.

### CPOL and CPHA — the four modes

Two settings decide when data is valid:

- **CPOL** (clock polarity): is the clock idle low (0) or idle high (1)?
- **CPHA** (clock phase): is data sampled on the first clock edge (0) or the second (1)?

| Mode | CPOL | CPHA | Sample on |
|---|---|---|---|
| 0 | 0 | 0 | rising edge |
| 1 | 0 | 1 | falling edge |
| 2 | 1 | 0 | falling edge |
| 3 | 1 | 1 | rising edge |

Modes 0 and 3 are the most common. The mode is in the slave's datasheet and must match
exactly. **Wrong mode is the classic SPI bug**: you get data, but shifted by one bit, or
consistent garbage. There is no acknowledgement to warn you, so the bus looks alive while
being wrong.

Also configurable: bit order (MSB first is usual) and clock speed.

### Failure modes

| Symptom | Likely cause |
|---|---|
| All zeros or all 0xFF | CS not asserted, or MISO not connected |
| Data shifted by one bit | wrong CPHA |
| Consistent garbage | wrong CPOL/CPHA combination |
| Works slow, fails fast | clock too high for the wiring, or slave's limit exceeded |
| Two slaves conflict | both CS lines low at once |

### Variants

- **Dual / Quad SPI** — 2 or 4 data lines instead of 1, used for flash memory. Your
  ESP32-S3 talks to its 16 MB flash chip over quad SPI.
- **QSPI / OSPI** for high-speed external memory
- **SDIO** — related but distinct, used for SD cards

---

## Choosing

Questions to ask, in order:

1. **How far?** More than a metre → UART with RS-485/RS-232, not I2C or SPI.
2. **How fast?** Megabytes per second → SPI. Kilobytes → any.
3. **How many devices?** Many, few pins → I2C. Few, need speed → SPI.
4. **Who initiates?** Slave needs to speak unprompted → UART, or SPI plus a separate
   interrupt line.
5. **Is the device already fixed?** Usually the sensor decides for you — read its
   datasheet first.

---

## Beyond the big three

| Protocol | Where |
|---|---|
| **CAN** | automotive, industrial. Differential pair, multi-master, message IDs instead of node addresses, built-in arbitration and error handling. ESP32-S3 has a TWAI controller; needs an external transceiver. |
| **RS-485** | industrial, long distance, multi-drop. Physical layer only — Modbus runs on top. |
| **1-Wire** | data and power on one wire. DS18B20 temperature sensors. |
| **I2S** | digital audio. Despite the name, unrelated to I2C. |
| **I3C** | I2C's successor: faster, in-band interrupts, dynamic addressing. |
| **USB** | far more complex; enumeration, descriptors, endpoints. |

---

## Debugging any bus

1. **Look at the signal.** A logic analyser turns a guessing game into reading. This is
   the single highest-value cheap tool in embedded work.
2. **Check the ground.** Two boards with no shared ground do not communicate.
3. **Check the levels.** 3.3 V and 5 V devices need level shifting.
4. **Slow it down.** If it works at low speed, the problem is signal integrity — wire
   length, capacitance, pull-up values — not your code.
5. **Verify the simplest possible transaction first.** I2C: scan the bus. SPI: read the
   chip's ID register. UART: loop TX back to RX. Prove the wiring before writing a driver.

---

## Interview questions that actually come up

- Why does I2C need pull-up resistors? *(open-drain — nobody drives high)*
- How does a UART receiver stay synchronised with no clock line? *(start-bit edge +
  oversampling, resynchronised every frame)*
- What are CPOL and CPHA and what happens if they are wrong?
- How do you put ten sensors on one bus with the fewest pins? *(I2C — but watch the
  parallel pull-ups and address collisions)*
- Two I2C devices with the same fixed address — what do you do? *(address-select pin, an
  I2C multiplexer, or a second bus)*
- Why is SPI faster than I2C? *(push-pull instead of open-drain, so much sharper edges;
  no per-byte acknowledgement; no addressing overhead)*
- What is clock stretching?
- How do you debug a bus where the device does not respond? *(the checklist above)*
