# Embedded Systems Learning Journey

Bare-metal and RTOS firmware practice on an **ESP32-S3** using **ESP-IDF v6.0.2**.

## Hardware

| Item | Detail |
|---|---|
| Board | LILYGO T-Display-S3 |
| MCU | ESP32-S3 (QFN56) rev v0.2, dual-core Xtensa LX7 @ 240 MHz |
| Flash | 16 MB, Winbond (external SPI chip) |
| PSRAM | 8 MB embedded |
| USB | Native USB-Serial/JTAG (no bridge chip, on-chip debugging available) |
| Display | 1.9" ST7789, 8-bit parallel i80 interface |

## Toolchain

- ESP-IDF v6.0.2 (`C:\esp\v6.0.2\esp-idf`)
- Build: CMake + Ninja via `idf.py`
- Flash/monitor: `idf.py flash monitor` on COM5
- Debug: on-chip USB JTAG via OpenOCD + GDB

## Projects

| Project | What it covers |
|---|---|
| `hello_world` | Toolchain bring-up, serial monitor, build size analysis, partition table |
| `blink` | GPIO output via driver API, then the same LED driven by direct register writes |
| `wokwi_iot_sensor` | Simulated IoT node (classic ESP32, Arduino + esp-mqtt): DHT22 + MPU6050 + OLED, web server, MQTT over WSS with TLS |
| `stm32_gpio_registers` | STM32F401 (NUCLEO-F401RE) without HAL: memory-mapped registers, masks, clock gating, GPIO output and input, ODR versus BSRR, SysTick. A tutorial with exercises. Different board and toolchain: see its own README |

## Notes

| File | Contents |
|---|---|
| `notes/day-01.md` | Toolchain bring-up, memory-mapped I/O, breadboard basics, debugging log |
| `notes/day-02.md` | Clock, hardware timers, PWM/LEDC, GPIO interrupts, switch bounce |
| `notes/theory/` | Hardware-independent reference: embedded C, serial protocols, interrupts and concurrency |

## Related repositories

Larger projects live in their own repositories:

| Repository | What it is |
|---|---|
| [`hava-istasyonu`](https://github.com/yavuzselimuyaner/hava-istasyonu) | ESP32-C3 weather station: KiCad board, ESP-IDF firmware, FreeCAD enclosure |
| [`stm32-uart-mouse`](https://github.com/yavuzselimuyaner/stm32-uart-mouse) | Five-button mouse on the STM32F401RE: timer-based polling, debounced clicks, interrupt-driven UART RX with a ring buffer |

## Build & run (ESP-IDF projects)

```
idf.py set-target esp32s3
idf.py menuconfig
idf.py flash monitor
```

`set-target` must be run once per project. It resets `sdkconfig`, so any menuconfig
settings have to be reapplied after it.
