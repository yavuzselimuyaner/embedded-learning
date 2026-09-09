# Filtering the big roadmap

The `m3y54m/Embedded-Engineering-Roadmap` repository lists ~40 sections and hundreds of
links, all presented as equally important. They are not — not for any one person, and not
at any one time.

This file maps that roadmap onto **this** situation: computer engineering graduate, strong
C / data structures / OS / computer architecture, no electronics course, working through
ESP-IDF on an ESP32-S3, aiming at embedded firmware roles.

Progress so far: GPIO and direct register access, hardware timers, PWM, GPIO interrupts,
debounce, UART. I2C blocked on unsoldered sensor headers.

---

## Blocking right now — go here today

| Roadmap section | Why |
|---|---|
| **Prototyping Skills → Soldering / Rework** | The sensor module shipped with unsoldered headers. This is literally the current blocker. The Digi-Key through-hole soldering video is the exact task. |
| **Using Test Equipment → Multimeter** | Two hardware faults so far were diagnosed by guesswork because there was no meter. SparkFun's guide takes ten minutes. |
| **Using Test Equipment → Logic / Protocol Analyzer** | Needed to actually *see* I2C and SPI rather than assume they work. |

---

## Currently in these — the active sections

| Roadmap section | Sub-items that matter now |
|---|---|
| **Microcontrollers** | GPIO ✓, Timers/Counters ✓, PWM ✓, Interrupts ✓, Clock Management ✓ · **next: ADC, DMA, Watchdog, Power Management, Bootloader/DFU** |
| **Interfaces & Protocols → Basic Protocols** | UART ✓ · **I2C and SPI next.** Only these three matter for now. |
| **Build System** | GCC, Make/CMake — already using them. Read *The Best and Worst GCC Compiler Flags For Embedded* and *Demystifying Firmware Linker Scripts*. |
| **Debugging** | GDB, OpenOCD, JTAG/SWD. Attempted, unresolved. Worth one more careful pass. |
| **Educational Websites** | Only two matter: **Interrupt (Memfault)** and **EmbeddedRelated**. Ignore the rest of that list. |

---

## Next, in order

1. **Sensors & Actuators** — once I2C works. Reading a sensor, scaling raw values,
   calibration.
2. **Memory Technologies & File Systems** — flash vs EEPROM vs SRAM, then NVS and
   littlefs on the ESP32.
3. **Operating Systems → RTOS Basics → FreeRTOS** — the biggest single win available,
   because the OS course maps onto it directly.
4. **Programming Fundamentals → State Machines, Design Patterns, Memory Management** —
   these are about *structuring* firmware. They matter more than another peripheral.
5. **Testing → TDD, Unit Testing** — before the final project, not after.
6. **CI/CD Pipelines** — a GitHub Actions workflow on the existing repo.

---

## Later, and only if the niche is chosen

| Section | When |
|---|---|
| **Edge AI / TinyML** | If the AI-flavoured route is taken. Strong fit with the DL and image-processing background. |
| **Automotive Protocols → CAN** | If the automotive/defence route is taken. Highest job count in Turkey. |
| **Standards & Certifications → Functional Safety, MISRA** | Same route. |
| **IoT, Delta OTA** | After OTA basics. |
| **Embedded GUI (LVGL)** | The board has a display; a good CV item, but not a core skill. |

---

## Skip — for now, with reasons

Not bad sections. Wrong stage, or wrong discipline.

| Section | Why skip |
|---|---|
| **FPGA Development** | A separate profession. Little overlap with firmware work. |
| **PCB Design / EMC**, **Hardware Design Basics** | Hardware engineer's track. |
| **Electronics** (the roadmap's book list) | The list is aimed at building an electronics engineer. Only the minimum is needed — see the electronics section in `resources.md`. |
| **Digital Signal Processing** | Needed for signal-processing roles only. |
| **Control Theory**, **MATLAB / Simulink** | Same. |
| **Embedded Linux** (Kernel, Yocto, Buildroot, U-Boot, drivers) | A different discipline, and a good one — but only after MCU work is solid. |
| **AUTOSAR** | Automotive only, and normally learned on the job. |
| **Embedded Security**, **Hardware Hacking**, **Cryptography** | Not the chosen direction. |
| **C++, Rust, Zig** | C first. One language, properly. |
| **Most protocol subsections** — MIPI CSI/DSI, HDMI, PCIe, Ethernet, SDIO, I3C, 1-Wire, I2S, PCM, LoRa, Zigbee, Thread, Matter, Modbus, EtherCAT, cellular | Learn these when a project needs one. Reading about a bus you will not touch teaches nothing that survives. |
| **Computer Architecture**, **Digital Design**, **Discrete Mathematics**, **Algorithms & Data Structures** | Already covered at university, with good grades. |
| **Arduino** | Deliberately skipped. ESP-IDF from the start. |
| **PlatformIO** | Not needed; ESP-IDF's own tooling is in use. |
| **SDLC Models**, **SVN** | Read in an afternoon when a job needs it. |

---

## How to use a roadmap at all

Not top to bottom. A roadmap is a **map**, not a syllabus — it tells you what exists and
roughly where it sits, so you can find the next thing and recognise what you are ignoring.

Three rules that keep it useful:

1. **One section at a time**, tied to something being built that week.
2. **Read a link when the topic blocks you**, not to "cover" it.
3. **Depth beats breadth.** A person who has truly implemented UART, I2C and SPI drivers
   beats one who has read about twenty protocols. Interviews find the difference in about
   two questions.

The roadmap's own warning says the same thing, and it is the most useful sentence in the
document: trying to study all of it leads to being tired and disappointed.
