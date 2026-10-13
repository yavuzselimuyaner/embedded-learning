# FAQ

## Why do we need bit operations at all?
A register is a single 32-bit number, but it holds settings for many pins side by side. The
CPU can only read or write the whole word. To change one pin you read the word, change only
your bits with AND/OR and a mask, and write it back. See [chapter 2](02-binary-hex-and-masks.md).

## Why is the PA5 field in MODER at bits 11:10?
Each pin takes 2 bits, and pin `n` has `n` pins before it, so its field starts at bit `2n`.
For pin 5 that is bit 10, covering bits 10 and 11. The field is always bits `2n` and `2n+1`:
pin 15 is bits 31:30, not 30:29 (bit 29 belongs to pin 14).

## Where do I find that in the manual?
RM0368, chapter 8 (GPIO), the description of `GPIOx_MODER`. It lists `MODER15[1:0]` at bits
31:30 down to `MODER0[1:0]` at bits 1:0, and the meaning of the four values.

## What does `3u` mean?
`3` is `0b11`: two bits set, which is the width of a MODER field. `3u << 10` marks bits 11 and
10. The `u` makes it unsigned.

## What does `1u << 5` evaluate to?
Bit 5 set, everything else zero: `0b0010 0000`, which is `32` or `0x20`. The left operand
is the value being shifted, the right operand is the distance. `5 << 1` is a different thing.

## Why do I clear the field and then OR the new value?
OR can only set bits. If the field held `10` and you OR in `01`, the result is `11`. Clear with
AND first so the field is `00`, then OR in the value.

## Why not just assign to MODER?
Assignment writes all 32 bits and resets the other 15 pins' modes as well. On GPIOA that
includes the debug pins, and the debugger would disconnect.

## Why `ODR |= x` is risky and `BSRR = x` is not?
`ODR |= x` is read, modify, write. If an interrupt changes the port in between, the write
overwrites its change with a stale value. `BSRR = x` is one write that only affects the bits
you set to 1. See [chapter 5](05-odr-vs-bsrr.md).

## So should I never use ODR?
Use it to write the whole port at once and to read the current output state. For setting or
clearing single pins use BSRR.

## Why is the clock enable the first thing in every program?
Peripherals are clock-gated after reset. A peripheral whose clock is off ignores register
writes silently, so configuring it first means the configuration is lost.

## Why is the button 0 when pressed?
The board has a pull-up resistor to 3.3 V and the button shorts the pin to ground. Released:
high, 1. Pressed: low, 0.

## Why PC13 and PA5? Can I use a different pin?
Those two pins are wired to the on-board button and LED by the PCB designers. For another pin
you attach your own LED (with a resistor) or button, and change the constants. An external
button needs a pull-up or pull-down, which you can enable in `GPIOx_PUPDR`.

## What does `volatile` do?
It tells the compiler the value can change outside the visible program flow, so every access
must really read or write memory. Registers and variables shared with interrupts need it.

## How is this different from HAL?
HAL functions wrap the same register writes. `HAL_GPIO_WritePin` ends up writing `BSRR`. HAL
saves you from looking up addresses and bit positions, and costs you a layer to debug through.
Knowing the registers tells you what a HAL call does and why it fails.

## Do I need CubeMX?
Not for these examples. CubeMX generates the clock, mode and pin setup code from a GUI. This
tutorial writes that code by hand to show what it is.

## Why can't the compiler just handle the 2n for me?
It can: CMSIS headers define names like `GPIO_MODER_MODER5_0`. The tutorial writes the
arithmetic out once so the names are not magic. For production code those headers are the
better choice.
