# STM32 bare-metal GPIO: a hands-on tutorial

Learn how a microcontroller is really controlled by driving an LED and reading a button on the
**NUCLEO-F401RE** without HAL, CubeMX or any vendor library. You write to the hardware
registers yourself, and every address and bit number is traced back to the reference manual.

If you have only used `HAL_GPIO_WritePin()` so far, this is what that function does underneath.

## Who this is for
- You know basic C: variables, functions, pointers, `#define`.
- You have not necessarily written embedded code before.
- You are comfortable reading a datasheet table, or want to become comfortable.

## What you will learn
- How peripherals are mapped into memory and how to find a register's address.
- Binary, hex, shifts and masks, as used on registers: set, clear, toggle, test, multi-bit fields.
- Why a peripheral's clock must be enabled before you configure it.
- Configuring a pin as output and input, and what the mode bits mean.
- Why `BSRR` exists and when it is safer than `ODR`.
- A hardware timer (SysTick) for delays instead of a busy loop.
- How to debug when nothing happens.

## Hardware and software
| What | Notes |
|---|---|
| Board | NUCLEO-F401RE (STM32F401RE, Cortex-M4). LD2 on PA5, user button B1 on PC13 |
| Compiler | `arm-none-eabi-gcc` (Arm GNU toolchain, or the copy inside STM32CubeIDE) |
| Flasher | STM32CubeProgrammer CLI, or OpenOCD, or `st-flash` with the on-board ST-LINK |
| Docs | RM0368 (reference manual), PM0214 (Cortex-M4 programming manual), UM1724 (Nucleo-64 user manual) |

Download the three documents from st.com and keep RM0368 open. The tutorial tells you where
to look rather than copying the tables, so you learn to find things yourself.

## Quick start
Windows with STM32CubeIDE installed (adjust `-ToolsRoot` if it lives elsewhere):

```powershell
.\build.ps1 -Example 01-led-blink -Flash
```

Any platform, plain command line:

```sh
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O1 -g -Wall -Wextra -nostdlib \
    -Icommon -T common/stm32f401re.ld \
    common/startup.c common/systick.c examples/01-led-blink/main.c \
    -o build/01-led-blink.elf
```
Then flash `build/01-led-blink.elf`, for example with
`STM32_Programmer_CLI -c port=SWD -w build/01-led-blink.elf -v -rst`.

## Learning path
Read the document, then run the matching example, then do the exercises.

| # | Document | Example |
|---|---|---|
| 1 | [Memory-mapped registers](docs/01-memory-mapped-registers.md) | |
| 2 | [Binary, hex and masks](docs/02-binary-hex-and-masks.md) | |
| 3 | [GPIO output: the LED](docs/03-gpio-output-led.md) | `examples/01-led-blink` |
| 4 | [GPIO input: the button](docs/04-gpio-input-button.md) | `examples/02-button-led` |
| 5 | [ODR versus BSRR](docs/05-odr-vs-bsrr.md) | |
| 6 | [SysTick and delays](docs/06-systick-and-delays.md) | `examples/03-blink-systick` |
| 7 | [Troubleshooting](docs/07-troubleshooting.md) | |
| | [FAQ](docs/faq.md) | |

## Repository layout
```
common/
  stm32f401_regs.h   register addresses used in the tutorial
  startup.c          reset handler and vector table
  stm32f401re.ld     linker script
  systick.c, .h      1 ms tick and delay_ms()
examples/
  01-led-blink/      LED on PA5, crude delay
  02-button-led/     LED follows the button (PC13)
  03-blink-systick/  1 Hz blink with a hardware timer
docs/                the tutorial
build.ps1            Windows build and flash helper
```

## A note on sources
Numbers in this tutorial come from RM0368, PM0214 and UM1724. Where something is easy to
get wrong it says which section to check. When the PDF and this text disagree, trust the PDF
and open an issue.
