# 7. Troubleshooting

Bare-metal failures are silent: the LED does not light and nothing tells you why. These are
the problems that come up most, and how to find them.

## The firmware on the board is not the firmware you just wrote
A failed build leaves the **previous** program on the chip, and the previous program may even
look right. Make a visible change before trusting a result:

1. Check the compiler output. There must be no error, and the `.elf` timestamp must be new.
2. Change something you can see (for example halve the blink delay) and flash.
3. If the behaviour did not change, the new code never reached the board.

## Checklist when a pin does nothing
Walk through it in order; each step is a register you can inspect in the debugger.

| Check | How |
|---|---|
| Is the port's clock enabled? | `RCC_AHB1ENR` bit for the port must be 1. Without it, writes to the port are ignored |
| Was the clock enabled **before** the pin was configured? | Order of the lines in `main` |
| Is the pin an output? | `GPIOx_MODER` bits `2n+1:2n` must be `01` |
| Are you driving the pin you think you are? | Print or inspect the constant, compare with the schematic (PA5 for LD2) |
| Does the mask follow the pin? | Derive masks from one `LED_PIN` constant, not literal `5` and `21` |
| Is the pin driven by another peripheral? | Alternate-function mode (`10`) hands the pin to a peripheral |

## Look at the registers
In STM32CubeIDE debug mode, open **Window → Show View → SFRs**, expand `GPIOA`, and look at
`MODER`, `ODR` and `IDR`. Edit a bit there and the pin changes at once. Doing that without
any code tells you whether the hardware and wiring are fine, so the bug must be in your code.

## Debugger connection lost
If the debugger suddenly cannot connect after you run your program, you probably rewrote
`GPIOA_MODER` with a plain assignment and switched PA13/PA14 (SWD pins) away from their debug
function. Hold the reset button while connecting, or use "connect under reset" in the
programmer, then fix the code to clear and set only your own bits.

## Things that look like bugs but are not
| Symptom | Reason |
|---|---|
| The LED looks dim | Switching faster than the eye resolves: it is on only part of the time |
| Button reads 0 when pressed | Active low (see chapter 4) |
| The loop with an empty body disappears | Compiler removed it: add `volatile` to what it waits on |
| The delay changes with `-O` flags | Busy loops are not timers: use SysTick |

## Windows specifics
- `build.ps1` is a PowerShell script. In `cmd.exe` call it as
  `powershell -ExecutionPolicy Bypass -File build.ps1 ...`.
- If the flasher reports the port is busy, close any serial terminal or IDE debug session
  that has the ST-LINK open.
