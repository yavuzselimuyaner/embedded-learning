# Embedded Systems Learning Journey

Hands-on firmware practice from toolchain bring-up to register-level drivers, on two
platforms: an **ESP32-S3** programmed with **ESP-IDF**, and an **STM32F401** (NUCLEO-F401RE)
programmed without any vendor library.

The repository is a learning log as much as a code collection. Each project explains what it
covers, and the notes record the questions asked and what actually went wrong while debugging.

## Contents

| Folder | Platform | Toolchain | What it covers |
|---|---|---|---|
| [`hello_world`](hello_world) | ESP32-S3 | ESP-IDF | Toolchain bring-up, serial monitor, build size analysis, partition table |
| [`blink`](blink) | ESP32-S3 | ESP-IDF | GPIO output via the driver API, then the same LED driven by direct register writes |
| [`uart_loopback`](uart_loopback) | ESP32-S3 | ESP-IDF | UART1 TX to UART2 RX on GPIO11 and GPIO2: framing, external loopback, what a baud-rate mismatch does to the data. Includes an internal loopback mode that needs no wire |
| [`i2c_scanner`](i2c_scanner) | ESP32-S3 | ESP-IDF | I2C bus scanner for a 10-DOF sensor board on GPIO11 (SDA) and GPIO2 (SCL). Currently finds no devices: blocked on unsoldered sensor headers, wiring or power fault not yet isolated |
| [`wokwi_iot_sensor`](wokwi_iot_sensor) | Simulated classic ESP32 | PlatformIO, Arduino framework, esp-mqtt | IoT node in the Wokwi simulator: DHT22, MPU6050 read through raw registers, SSD1306 OLED, web server, MQTT over WSS with a pinned root CA. Written as a tutorial |
| [`stm32_gpio_registers`](stm32_gpio_registers) | NUCLEO-F401RE | arm-none-eabi-gcc | LED and button without HAL: memory-mapped registers, binary/hex and masks, clock gating, ODR versus BSRR, SysTick. A tutorial with exercises |
| [`notes`](notes) | none | none | Daily notes and hardware-independent theory |

## Suggested reading order

1. [`notes/day-01.md`](notes/day-01.md): toolchain, memory-mapped I/O, breadboard rules.
2. `hello_world`, then `blink`.
3. [`notes/day-02.md`](notes/day-02.md): clocks, timers, PWM, GPIO interrupts, switch bounce.
4. `uart_loopback` with [`notes/theory/protocols.md`](notes/theory/protocols.md), then `i2c_scanner`.
5. [`notes/theory/interrupts-and-concurrency.md`](notes/theory/interrupts-and-concurrency.md),
   then `wokwi_iot_sensor`.
6. [`stm32_gpio_registers`](stm32_gpio_registers): the register-level ideas from `blink`, taught from scratch
   on a different chip.

## Hardware

### ESP32-S3 board

| Item | Detail |
|---|---|
| Board | LILYGO T-Display-S3 |
| MCU | ESP32-S3 (QFN56) rev v0.2, dual-core Xtensa LX7 @ 240 MHz |
| Flash | 16 MB, Winbond (external SPI chip) |
| PSRAM | 8 MB embedded |
| USB | Native USB-Serial/JTAG (no bridge chip, on-chip debugging available) |
| Display | 1.9" ST7789, 8-bit parallel i80 interface |

### STM32 board

| Item | Detail |
|---|---|
| Board | NUCLEO-F401RE |
| MCU | STM32F401RE, Arm Cortex-M4, 512 KB flash, 96 KB RAM |
| On board | ST-LINK programmer and debugger, user LED LD2 on PA5, user button B1 on PC13 |

### Simulated

`wokwi_iot_sensor` runs in the [Wokwi](https://wokwi.com) simulator on a classic ESP32 DevKit
with virtual sensors, so it needs no hardware.

## Toolchains

| Projects | Tools |
|---|---|
| ESP32-S3 (`hello_world`, `blink`, `uart_loopback`, `i2c_scanner`) | ESP-IDF v6.0.2 (`C:\esp\v6.0.2\esp-idf`); build with CMake and Ninja via `idf.py`; flash and monitor with `idf.py flash monitor` on COM5; debug over on-chip USB JTAG with OpenOCD and GDB |
| STM32 (`stm32_gpio_registers`) | `arm-none-eabi-gcc` and `STM32_Programmer_CLI` (both ship with STM32CubeIDE); see the folder's README |
| Simulation (`wokwi_iot_sensor`) | PlatformIO and the Wokwi extension; see section 11 of the folder's README |

## Build and run (ESP-IDF projects)

```
idf.py set-target esp32s3
idf.py menuconfig
idf.py flash monitor
```

`set-target` must be run once per project. It resets `sdkconfig`, so any menuconfig
settings have to be reapplied after it.

## Notes

| File | Contents |
|---|---|
| [`notes/day-01.md`](notes/day-01.md) | Toolchain bring-up, memory-mapped I/O, breadboard basics, debugging log |
| [`notes/day-02.md`](notes/day-02.md) | Clock, hardware timers, PWM/LEDC, GPIO interrupts, switch bounce |
| [`notes/theory/`](notes/theory/README.md) | Hardware-independent reference, listed below |

Theory files, each ending with self-test questions:

| File | Contents |
|---|---|
| [`embedded-c.md`](notes/theory/embedded-c.md) | Fixed-width types, bit manipulation, `volatile`, `static`, register access patterns, callbacks, why `malloc` is avoided, integer pitfalls, ring buffers, compiler flags |
| [`protocols.md`](notes/theory/protocols.md) | UART, I2C and SPI: framing, wiring, failure modes, how to choose |
| [`interrupts-and-concurrency.md`](notes/theory/interrupts-and-concurrency.md) | Interrupt mechanics, latency, ISR rules, atomicity, race conditions, critical sections, debouncing, RTOS mapping |
| [`roadmap-filter.md`](notes/theory/roadmap-filter.md) | A large community roadmap filtered down to what matters now, next, later and never |
| [`resources.md`](notes/theory/resources.md) | Books, courses and blogs ordered by when to use them, plus ways to practise from a PC |

## Related repositories

Larger projects live in their own repositories:

| Repository | What it is |
|---|---|
| [`hava-istasyonu`](https://github.com/yavuzselimuyaner/hava-istasyonu) | ESP32-C3 weather station: KiCad board, ESP-IDF firmware, FreeCAD enclosure |
| [`stm32-uart-mouse`](https://github.com/yavuzselimuyaner/stm32-uart-mouse) | Five-button mouse on the STM32F401RE: timer-based polling, debounced clicks, interrupt-driven UART RX with a ring buffer |
