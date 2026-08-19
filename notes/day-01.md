# Embedded Notes — Day 1

ESP32-S3 (LILYGO T-Display-S3) · ESP-IDF v6.0.2 · Windows

---

## 1. The hardware, precisely

Four different things people casually call "the chip":

| Part | Where | Size | Volatile? |
|---|---|---|---|
| ESP32-S3 | main package | 2 cores, 240 MHz | — |
| SRAM (DIRAM) | **inside** the MCU | ~342 KB | yes, lost on power-off |
| Flash | **separate Winbond chip** | 16 MB | no, keeps data |
| PSRAM | separate die | 8 MB | yes |

Analogy: flash = SSD, SRAM = RAM, ESP32-S3 = CPU. The MCU talks to the flash over SPI, exactly like it will talk to a sensor later.

The ESP32-S3 executes code **directly from flash through a cache** (execute-in-place). That is why `idf.py size` reports both `Flash Code .text` and `DIRAM .text` — some code runs from flash, timing-critical code is copied into internal RAM.

`IRAM 100% used` in the size report is normal, not an error.

---

## 2. Toolchain — command reference

Everything runs in the **IDF PowerShell environment**, not a plain shell.

| Command | Purpose |
|---|---|
| `idf.py set-target esp32s3` | Set the target chip. **Once per project.** Resets `sdkconfig`. |
| `idf.py menuconfig` | Edit build-time settings (writes `sdkconfig`) |
| `idf.py build` | Compile |
| `idf.py flash monitor` | Compile + upload + open serial monitor |
| `idf.py size` / `size-components` | Flash and RAM usage of the built binary |
| `idf.py partition-table` | Show how flash is divided |
| `esptool -p COM5 flash-id` | Ask the chip about its flash chip and size |

Exit the monitor with **`Ctrl + ]`**. Pressing `Ctrl+C` throws a Python traceback.

### Three tools, three different jobs

- **`esptool`** — low-level serial tool. Talks to the chip: read/write/erase flash, read chip ID. Fixed set of subcommands. **Knows nothing about projects.**
- **`idf.py`** — the build tool. Operates on **the project in the current directory**. You `cd` into a project; you never pass the project name as an argument.
- **A project** (`hello_world`, `blink`) — a *folder*, not a command.

### Project layout

```
blink/
├── CMakeLists.txt      project definition
├── sdkconfig           what menuconfig writes
└── main/
    ├── CMakeLists.txt
    └── blink_example_main.c    <- the code, contains app_main()
```

Never build the examples in place inside the IDF installation. Copy them out first.

---

## 3. Memory-mapped I/O — the core concept of Day 1

`gpio_set_level(pin, 1)` is a driver function. Underneath, it writes a number to a **specific memory address**. Peripherals (GPIO, timers, UART) are wired to fixed addresses; writing there does not store data, it drives hardware.

A register is a **32-bit box where each bit maps to one pin**:

```
bit:    ...  4      3      2      1      0
pin:    ...  GPIO4  GPIO3  GPIO2  GPIO1  GPIO0
```

`(1 << 2)` means "shift 1 left by 2 places":

```
1         =  0000 0001
(1 << 2)  =  0000 0100
                   ^ bit 2
```

### The three GPIO output registers

| Register | Effect |
|---|---|
| `GPIO_OUT_REG` | full 32-bit output state |
| `GPIO_OUT_W1TS_REG` | **write 1 to set** — bits that are 1 go HIGH, bits that are 0 are untouched |
| `GPIO_OUT_W1TC_REG` | **write 1 to clear** — bits that are 1 go LOW, bits that are 0 are untouched |

So `REG_WRITE(GPIO_OUT_W1TS_REG, (1 << 2))` means exactly: *"drive GPIO2 high, do not touch the other 31 pins."*

ESP32-S3 has more than 32 GPIOs, so there is a second set (`OUT1_W1TS`) for GPIO32+.

### Why W1TS/W1TC exist — atomicity

`OUT |= (1 << 1)` is **three operations**: read, modify, write. If an interrupt fires in between and changes a different pin, writing back the stale value erases its change.

Writing to `W1TS` is **one operation** and cannot be interrupted halfway. That is what *atomic* means. The same problem reappears in ISRs (Block 3) and in FreeRTOS shared state (Block 6) — learning it here on one LED is the cheapest possible version.

### Why `volatile`

The compiler sees repeated writes to the same address and concludes they are redundant, so it optimises them away. `volatile` tells it: *this address changes for reasons you cannot see — do not touch it.* Provable by compiling a busy-wait loop with `-O2`.

---

## 4. Documentation — which document answers which question

| Document | Answers |
|---|---|
| **Datasheet** | electrical: pinout, voltages, current limits, temperature. *"How many mA can this pin source?"* |
| **Technical Reference Manual (TRM)** | register map, peripheral internals. *"What happens if I write to this register?"* |

The TRM is ~1500 pages. Nobody reads it front to back — **`Ctrl+F` it**. Searching `GPIO_OUT_W1TS_REG` lands directly on the register table with its address and bit meanings.

Every MCU family has its own TRM, all organised the same way. The same reflex will apply to STM32's RM0008 later.

---

## 5. Breadboard rules

- The **row number is the electrical node**. `1a`, `1b`, `1c`, `1d`, `1e` are all the same point.
- The **letter does not matter** — it only picks which physical hole.
- The **centre channel is a wall**: `1a` and `1f` are *not* connected.
- Different rows are never connected: `1a` and `2a` are separate.
- The long `+` / `-` rails along the edges are for distributing power to many components. With a single LED you do not need them at all.
- **A component's two legs must never sit in the same row** — the row is already shorted internally, so the component gets bypassed.
- On large breadboards the power rails are often **split in the middle**. Check the printed line before trusting a rail end to end.

### LED circuit

```
GPIO -> 330 ohm -> LED long leg (anode)
                   LED short leg (cathode) -> GND
```

- LED is polarised: **long leg = anode = positive**. Flat edge on the plastic rim = cathode.
- Resistors are **not** polarised, either orientation works.
- Resistor colour bands: **330 ohm = orange, orange, brown**. **10K = brown, black, orange.** Easy to mix up; a 10K here gives ~0.13 mA and the LED stays dark.
- Forward voltage matters: a yellow LED (Vf ~2.0 V) leaves 1.3 V across 330 ohm → ~4 mA. A green/blue/white LED (Vf ~3.0 V) on a 3.3 V rail leaves almost nothing → invisible.
- `3V` and `3V3` are the same pin, just different silkscreen labels.

---

## 6. Questions I asked, and the answers

**Is "the monitor" the computer screen?**
Yes — the PowerShell window. Nothing to do with the board's LCD. The board sends text over USB (`printf` / `ESP_LOGI`) and `idf.py monitor` displays it. On boards without a screen this is the only debugging output you get.

**Do I check code size with C code, or from PowerShell?**
Neither — the compiler already computed it, you just ask. Three different questions, three different places:
- How big is my code? → `idf.py size`
- How is flash divided? → `idf.py partition-table`
- How much flash does the board have? → `esptool flash-id`

**Is "flash" the microcontroller?**
No. It is a separate memory chip (Winbond here, manufacturer ID `ef`).

**Why does esptool hard-reset every time?**
To talk to the chip, esptool must first put it into **download mode**, where the application does not run. When finished it resets the chip so the application starts again. Without that reset the board would just sit there.

"via RTS pin" is a legacy name: on classic boards a USB-UART bridge chip's RTS/DTR lines are wired to EN and GPIO0 to toggle boot mode automatically. This board has no bridge chip (native USB), but esptool emulates the same behaviour and kept the old message. Override with `--after no-reset` / `--before no-reset`.

**Express or Custom install?**
Express. Custom only lets you pick the directory and version, and there was no reason to care about either. Keep the install path short and free of spaces.

**Does the breadboard column letter matter?**
No. Only the row number. See section 5.

---

## 7. Debugging log — what actually went wrong

The most useful part of the day. Every one of these cost real time.

**1. "smartconfig / connection timed out" on first plug-in**
Not my code — the factory firmware talking. It proved the board, cable and serial link all worked. Lesson: identify *whose* code is producing the output before debugging it.

**2. `flash-id` → "not recognized as a cmdlet"**
`flash-id` is a **subcommand** of esptool, not a standalone command: `esptool flash-id`.

**3. `esptool hello-world` → "No such command"**
Conceptual mix-up. `esptool` talks to the chip, `idf.py` builds projects, and `hello_world` is a folder. You `cd` into the folder and run `idf.py` there.

**4. LED would not light even on 3V3**
Root cause: **the board was not fully seated in the breadboard.** The resistor and the LED were correct from the start.

Lesson: when something on a breadboard does not work, suspect **contact** before suspecting components. Isolate hardware from software first — test the LED circuit on a static 3.3 V rail before blaming any code.

**5. Code ran, LED did not blink**
The jumper was still on `3V` instead of a GPIO. A permanently-on LED and a permanently-off LED point at different faults — always note which one you have.

**6. Source edits had no effect on the board**
Root cause: **`idf.py set-target esp32s3` was never run in the `blink` project.** It was building for plain `esp32` and every flash failed with `This chip is ESP32-S3, not ESP32` — so the chip kept running an older binary while I kept editing source.

Lessons:
- `set-target` is **per project**. Each project has its own `sdkconfig`.
- `set-target` **wipes `sdkconfig`**, so menuconfig settings must be reapplied after it.
- To prove your code actually reaches the chip, change a log string and look for it in the monitor. If the new text does not appear, you are editing a file that is not being flashed.

**7. LED stayed permanently on after the register rewrite**
`blink_led()` only contained the `W1TS` write — no `W1TC`, and `s_led_state` was never read. "Always on" means the set path runs and the clear path does not.

**8. Flash backup aborted: "Packet content transfer stopped"**
Long reads over the ESP32-S3's native USB-JTAG are flaky at high baud with the stub flasher. Dropping `-b`, adding `--no-stub` and reading a smaller region is more stable. Not worth much time — LilyGO publishes the factory firmware anyway.

**9. Flash configured as 2 MB when the board has 16 MB**
Default `sdkconfig` assumed 2 MB. The boot log said so twice: `SPI Flash Size : 2MB` and `W spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k)`. Fixed in menuconfig. Will matter for OTA later, which needs two app partitions.

---

## 8. Code quality note

Do not hardcode the pin number in the register write:

```c
REG_WRITE(GPIO_OUT_W1TS_REG, (1 << 1));           // bad
REG_WRITE(GPIO_OUT_W1TS_REG, (1 << BLINK_GPIO));  // good
```

`configure_led()` sets the direction of `BLINK_GPIO`, which comes from menuconfig. If the register write hardcodes a different number, the driver configures one pin while you write to another. **Never keep the same fact in two places.**

---

## 9. Still open

- [ ] Restore or re-download the LilyGO factory firmware
- [ ] Run the debugger: breakpoint over on-chip USB JTAG, read a variable
- [ ] CPU still runs at 160 MHz; 240 MHz is a menuconfig setting
- [ ] PSRAM (8 MB) is not enabled yet
- [ ] Buy: logic analyzer (needed mid-Block 4), multimeter, microSD module for SPI

## 10. Next — Block 3

Clock tree, SysTick, hardware timers, PWM (LEDC), GPIO interrupts, `IRAM_ATTR`, button debounce.
