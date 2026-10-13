# 6. SysTick and delays

Example: [`examples/03-blink-systick`](../examples/03-blink-systick/main.c).

## Why not a counting loop
Example 01 delays with `for (volatile i = 0; i < 400000; i++);`. How long that takes depends on
the clock, the compiler's optimisation level and even the exact instructions it chose. Change
`-O1` to `-O0` and the blink rate changes. For real timing you need a hardware timer.

## SysTick
Every Cortex-M core has a 24-bit down-counter called SysTick. It counts core clock cycles,
reloads when it reaches zero, and can raise an interrupt each time. That interrupt becomes the
system's millisecond heartbeat.

Three registers (PM0214, SysTick chapter):

| Register | Address | Purpose |
|---|---|---|
| `SYST_CSR` | `0xE000E010` | control and status |
| `SYST_RVR` | `0xE000E014` | reload value |
| `SYST_CVR` | `0xE000E018` | current value (any write clears it) |

`SYST_CSR` bits:

| Bit | Name | Meaning |
|---|---|---|
| 0 | ENABLE | counter runs |
| 1 | TICKINT | interrupt when the counter reaches 0 |
| 2 | CLKSOURCE | 1 = processor clock |
| 16 | COUNTFLAG | set when counted to 0 (read-only) |

`SYST_CSR = 0x7` sets bits 0, 1 and 2.

## Choosing the reload value
After reset the chip runs from the internal 16 MHz oscillator (HSI). The counter counts
`RVR + 1` cycles per interrupt:

```
interrupts per second = 16 000 000 / (RVR + 1)
RVR = 16 000 - 1  →  1000 interrupts per second  →  one every millisecond
```

## The tick counter
```c
static volatile uint32_t ms_ticks;

void SysTick_Handler(void) { ms_ticks++; }
```
The handler's name is fixed by the vector table in `startup.c`. `ms_ticks` is `volatile`
because the interrupt changes it behind the main loop's back.

## A delay that survives wrap-around
```c
void delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms) { }
}
```
Subtracting unsigned numbers gives the elapsed time even when the counter wraps from
`0xFFFFFFFF` to `0`:

```
start = 0xFFFFFFF0, now = 0x00000010
now - start = 0x20 = 32 ms      (correct, modulo 2^32)
```
Comparing `ms_ticks < start + ms` instead would fail in that situation.

## Limits
`delay_ms` blocks the CPU: nothing else runs meanwhile. It is fine for a blink demo. In larger
programs you check `millis()` against a deadline in a loop and do other work in between, or
use a scheduler.

## Exercises
1. How would you change the reload value for a 100 Hz tick?
2. Blink two LEDs at different rates (500 ms and 750 ms) without blocking in `delay_ms`. Use
   `millis()` and a "last toggled" variable for each.
3. Use `millis()` to ignore button changes for 30 ms after the first edge.

<details><summary>Answers</summary>

1. `RVR = 160000 - 1`.
2. Keep two timestamps. In the loop: `if (millis() - t1 >= 500) { toggle LED1; t1 = millis(); }`
   and the same for the second LED.
3. Store the time of the last accepted change; ignore changes while
   `millis() - last < 30`.
</details>

Next: [Troubleshooting](07-troubleshooting.md)
