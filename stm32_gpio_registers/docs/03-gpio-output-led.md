# 3. GPIO output: the LED

Goal: make the green LED (LD2) blink. Example: [`examples/01-led-blink`](../examples/01-led-blink/main.c).

## What the board gives you
The Nucleo's green user LED LD2 is wired to **PA5**: port A, pin 5 (UM1724). The wiring is fixed
on the PCB. To use another colour, you wire an external LED with a series resistor to another pin
and change the pin number.

To drive a pin as an output you need two things, in this order:
1. Enable the **clock** of the port.
2. Set the pin's **mode** to output.

Then you write the level.

## Step 1: the clock
After reset most peripherals have their clock gated off to save power, and a peripheral
**ignores register writes while its clock is off**. The reset and clock control unit (RCC) has one
enable bit per peripheral. GPIO ports are on the AHB1 bus, so the register is `RCC_AHB1ENR`
(offset `0x30`, RM0368 chapter 6):

| Bit | Name | Meaning |
|---|---|---|
| 0 | GPIOAEN | clock for GPIOA |
| 1 | GPIOBEN | clock for GPIOB |
| 2 | GPIOCEN | clock for GPIOC |

```c
RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;    /* set bit 0, leave the other bits alone */
```

## Step 2: the mode
`GPIOx_MODER` configures the direction of all 16 pins in a single 32-bit register. Each pin
gets **two bits**, because there are four modes:

| Bits | Mode |
|---|---|
| 00 | input |
| 01 | general purpose output |
| 10 | alternate function (UART, PWM, ...) |
| 11 | analog |

### Where is pin n? The 2n rule
The fields are packed from the right, two bits per pin:

```
bit:   31 30 | 29 28 | 27 26 | ...... | 11 10 |  9  8 | ... |  3  2 |  1  0
pin:     15   |   14  |   13  |        |   5   |   4   |     |   1   |   0
```

There are `n` pins before pin `n`, each taking 2 bits, so pin `n`'s field starts at bit `2n` and
covers bits `2n` and `2n+1`:

| Pin | Field |
|---|---|
| 0 | bits 1:0 |
| 5 | bits 11:10 |
| 13 | bits 27:26 |
| 15 | bits 31:30 |

(This is what RM0368 shows as `MODER5[1:0]` at bits 11:10.)

### Writing the mode
```c
GPIOA_MODER &= ~(3u << (LED_PIN * 2u));   /* clear the field    */
GPIOA_MODER |=  (1u << (LED_PIN * 2u));   /* write 01 = output  */
```

Worked example for PA5 (`LED_PIN * 2 = 10`), starting from the reset value `0xA8000000`:

| Step | Expression | Value |
|---|---|---|
| 1 | `3u` | `0b11` |
| 2 | `3u << 10` | `0x00000C00` (bits 11 and 10) |
| 3 | `~(3u << 10)` | `0xFFFFF3FF` |
| 4 | `MODER & 0xFFFFF3FF` | `0xA8000000` (bits 11:10 were already 00) |
| 5 | `1u << 10` | `0x00000400` |
| 6 | `MODER \| 0x400` | `0xA8000400` (PA5 field is now 01) |

### Why not `GPIOA_MODER = ...`?
A plain assignment rewrites all 32 bits. The reset value of `GPIOA_MODER` is `0xA8000000`
(RM0368, section 8.4.1): pins PA13, PA14 and PA15 are in alternate-function mode because they
carry the debug interface (SWDIO, SWCLK, JTDI).

```
0xA8000000 = 1010 1000 0000 ...
             ^^            PA15 = 10
               ^^          PA14 = 10
                 ^^        PA13 = 10
```
Overwriting the register would switch them to input mode and your debugger would lose its
connection to the chip. Clear-then-set touches exactly two bits.

### Why clear before setting?
If the field held `10` and you only OR-ed in `01`, the result would be `11` (analog mode). The
two bits have to be zero before the new value goes in.

## Step 3: set and reset the pin with BSRR
`GPIOx_BSRR` is a write-only register with two halves:

| Bits | Effect when you write 1 |
|---|---|
| 0 to 15 | **set** pin n (drive it high) |
| 16 to 31 | **reset** pin n-16 (drive it low) |

Writing 0 to a bit does nothing. So one write changes one pin and leaves every other pin alone:

```c
GPIOA_BSRR = (1u << LED_PIN);           /* PA5 high: LED on  */
GPIOA_BSRR = (1u << (LED_PIN + 16u));   /* PA5 low:  LED off */
```
Why this beats read-modify-write on `ODR` is covered in [chapter 5](05-odr-vs-bsrr.md).

## Putting it together
```c
RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
GPIOA_MODER &= ~(3u << (LED_PIN * 2u));
GPIOA_MODER |=  (1u << (LED_PIN * 2u));

while (1) {
    GPIOA_BSRR = (1u << LED_PIN);
    crude_delay();
    GPIOA_BSRR = (1u << (LED_PIN + 16u));
    crude_delay();
}
```

## Experiments
1. **Break the order.** Move the clock-enable line below the two MODER lines, rebuild, flash. The
   LED never lights. The MODER writes arrived while GPIOA had no clock and were discarded.
   When the clock came on afterwards, MODER still held its reset value. Put the line back.
2. **Skip the clear.** Remove the `&= ~(...)` line. On a pin that resets to input this still
   works (the field is already 00), but think about what happens on PA13.
3. **Change the pin.** Wire an external LED with a 220 to 330 ohm resistor to PA6 (Arduino header
   D12) and ground. Change `LED_PIN` to 6. Every mask follows because they are all derived from the
   constant.

## Exercises
1. Which bits of `GPIOB_MODER` hold the mode of PB3? What is the mask to clear them?
2. Write the two lines that make PA7 an output.
3. Write the BSRR value that switches PA7 off.

<details><summary>Answers</summary>

1. `2 × 3 = 6`, so bits 7:6. Mask: `~(3u << 6)`.
2. `GPIOA_MODER &= ~(3u << 14); GPIOA_MODER |= (1u << 14);`
3. `1u << (7 + 16)` = `0x00800000`.
</details>

Next: [GPIO input: the button](04-gpio-input-button.md)
