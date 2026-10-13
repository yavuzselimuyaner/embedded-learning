# 5. ODR versus BSRR

Both registers change output pins. They are not interchangeable.

| | `ODR` (output data register) | `BSRR` (bit set/reset register) |
|---|---|---|
| Holds | the output level of all 16 pins | nothing: it is a command register |
| Read | yes | no (reads back 0) |
| Write | all 16 bits at once | bits 0-15: set pin; bits 16-31: reset pin |
| Bits written as 0 | pin is driven low | **ignored** |
| Internally | the state | changes the bits of ODR |

## The problem with ODR
To set one pin through ODR you write:

```c
GPIOA_ODR |= (1u << 5);
```
That one line is **three** operations: read ODR, OR with the mask, write the result back. If an
interrupt changes the same port between the read and the write, its change is lost.

Concrete trace. ODR starts at `0x00`. The main code sets PA5. An interrupt handler sets PA6 at
the wrong moment:

| Step | Who | Action | Real ODR |
|---|---|---|---|
| 1 | main code | reads ODR, gets `0x00` | `0x00` |
| 2 | interrupt | `ODR \|= (1u << 6)` (PA6 on) | `0x40` |
| 3 | main code | computes `0x00 \| 0x20 = 0x20` from its stale copy | `0x40` |
| 4 | main code | writes `0x20` | **`0x20`** |

The value written in step 4 was computed from the copy read in step 1, which does not contain
the interrupt's change. PA6 is switched off. The correct result, `0x60`, never existed. This
kind of bug is called a **race condition**. It is rare, depends on timing, and is very hard to
find.

## BSRR avoids it
```c
GPIOA_BSRR = (1u << 5);          /* set PA5   */
GPIOA_BSRR = (1u << (5 + 16));   /* reset PA5 */
```
It is a single write. There is no stale copy to write back, and every bit you leave at 0 is
ignored, so other pins are not touched no matter what runs in between. The interrupt can fire
at any point and its effect stays.

## So which one do I use?
- **Setting or clearing individual pins:** `BSRR`. This is the default.
- **Writing the whole port at once** (for example an 8-bit parallel bus): `ODR = value;`.
- **Knowing the current output state** (for example to toggle): read `ODR`.

A toggle with BSRR reads ODR to decide, then writes the set or the reset bit:

```c
if (GPIOA_ODR & (1u << LED_PIN)) {
    GPIOA_BSRR = (1u << (LED_PIN + 16u));
} else {
    GPIOA_BSRR = (1u << LED_PIN);
}
```
If you can get an interrupt that touches the same pin, the toggle needs its own protection.
That is not a BSRR problem, it is a "read then act" problem, and it is the topic of critical
sections.

## HAL does the same
`HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)` compiles to
`GPIOA->BSRR = GPIO_PIN_5`, and the reset case to `GPIOA->BSRR = GPIO_PIN_5 << 16`.

## Exercises
1. Write one BSRR value that turns PA5 on and PA6 off in a single write.
2. Why does `GPIOA_BSRR |= (1u << 5)` not do what it looks like?
3. Describe an interrupt scenario where `ODR ^= (1u << 5)` loses an update.

<details><summary>Answers</summary>

1. `GPIOA_BSRR = (1u << 5) | (1u << (6 + 16));`
2. `|=` reads the register first, and BSRR reads back 0, so it writes `0 | mask`. It works by
   accident here, but the read is pointless, and for the reset half you would not get the old
   state. Use a plain `=`.
3. Main reads ODR; an interrupt changes bit 5 or another bit of the same port; main writes the
   stale XOR result back, undoing the interrupt's change.
</details>

Next: [SysTick and delays](06-systick-and-delays.md)
