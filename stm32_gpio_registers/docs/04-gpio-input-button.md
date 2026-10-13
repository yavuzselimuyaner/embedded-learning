# 4. GPIO input: the button

Goal: light the LED while the blue user button is held. Example:
[`examples/02-button-led`](../examples/02-button-led/main.c).

## What the board gives you
The user button **B1** is wired to **PC13** (port C, pin 13) (UM1724). Because the LED is on
port A and the button on port C, you must enable two clocks.

## Reading a pin: IDR
`GPIOx_IDR` (offset `0x10`) is a read-only register. Bit n reflects the current voltage level
of pin n: 1 for high, 0 for low. MODER does not store the button state, it only selects
whether the pin is an input. The state is always read from IDR.

```c
GPIOC_MODER &= ~(3u << (BUTTON_PIN * 2u));       /* 00 = input */
...
if ((GPIOC_IDR & (1u << BUTTON_PIN)) == 0u) { ... }
```

`GPIOC_IDR & (1u << 13)` keeps only bit 13, so the result is `0` or `0x2000`. An equivalent
form is `(GPIOC_IDR >> 13) & 1u`, which gives `0` or `1`.

After reset a pin is already an input, so the MODER line is not strictly required. Write it
anyway: the code then says what you intend, and it keeps working if the pin was configured
differently earlier.

## Active low
The button reads **0 while pressed**, not 1. The board has a pull-up resistor from PC13 to
3.3 V and the button connects the pin to ground:

```
 3.3 V ──[ pull-up ]──┬── PC13
                      │
                   [ B1 ]
                      │
                     GND

released:  PC13 pulled up to 3.3 V      → IDR bit 13 = 1
pressed:   PC13 shorted to ground       → IDR bit 13 = 0
```
So "pressed" means "bit is 0". Signals that are active when low are common and are called
**active low**. (Check the exact wiring in the UM1724 schematic.)

## The program
```c
RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;

GPIOA_MODER &= ~(3u << (LED_PIN * 2u));
GPIOA_MODER |=  (1u << (LED_PIN * 2u));          /* PA5 output */
GPIOC_MODER &= ~(3u << (BUTTON_PIN * 2u));       /* PC13 input */

while (1) {
    if ((GPIOC_IDR & (1u << BUTTON_PIN)) == 0u) {
        GPIOA_BSRR = (1u << LED_PIN);            /* pressed  */
    } else {
        GPIOA_BSRR = (1u << (LED_PIN + 16u));    /* released */
    }
}
```
This is **polling**: the loop asks the pin over and over. It is simple and fine for a first
program. It wastes CPU time, and a long-running task elsewhere in the loop would make the
button feel unresponsive. Interrupts (EXTI) solve that and are a later topic.

## Using your own button on another pin
The wiring on the board is fixed, so PC13 cannot be swapped for another pin. If you connect
your own button, two things change:

1. Use the pin you chose in the `BUTTON_PIN` constant and the right port's `MODER`/`IDR`.
2. The board's pull-up is not there. A floating input reads random values. Enable the chip's
   internal pull-up in `GPIOx_PUPDR` (offset `0x0C`, two bits per pin: `00` none, `01` pull-up,
   `10` pull-down), and wire the button between the pin and ground.

## Bouncing
Mechanical contacts do not switch cleanly. For a few milliseconds the level flickers between
0 and 1. For "LED follows button" that is harmless. For "toggle on press" it is not: one press
can register as several. The usual fix is to ignore changes for 10 to 50 ms after the first one,
using a millisecond timer (see [chapter 6](06-systick-and-delays.md)).

## Exercises
1. Make the LED light when the button is **released** instead.
2. Make the LED toggle on each press without debounce. How many times does it toggle per
   press? Add a 30 ms ignore window using `millis()` and compare.
3. Read the MODER reset value of GPIOC in RM0368 and explain why the input line is not strictly
   necessary.

Next: [ODR versus BSRR](05-odr-vs-bsrr.md)
