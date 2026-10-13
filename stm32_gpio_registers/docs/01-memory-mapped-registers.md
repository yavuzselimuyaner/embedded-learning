# 1. Memory-mapped registers

## The idea
The CPU in an STM32 has one address space. Some addresses point to RAM, some to flash, and
some to **peripherals**: the GPIO ports, timers, UARTs. Reading or writing such an address does
not touch memory. It talks to hardware.

A **register** is a 32-bit word at one of these addresses. Each bit (or small group of bits)
is wired to something: a switch, a status flag, a pin level.

```
0x0800 0000  flash        your program
0x2000 0000  SRAM         variables, stack
0x4002 0000  GPIOA        ← writing here changes a pin
0x4002 3800  RCC          ← writing here enables clocks
0xE000 E010  SysTick      ← part of the CPU core
```

## Finding a register's address
Every register address is **peripheral base address + register offset**. Two tables give you
both numbers.

1. **Base address.** RM0368 chapter 2.3 "Memory map" has a table of peripheral boundary
   addresses. GPIOA starts at `0x4002 0000`, GPIOB at `0x4002 0400`, GPIOC at `0x4002 0800`.
   Each GPIO port occupies `0x400` bytes. The table also tells you which bus the peripheral is on
   (GPIO is on AHB1), which matters for clocks.
2. **Offset.** Each peripheral chapter ends with a register map. For GPIO (RM0368 chapter 8):

   | Register | Offset | Purpose |
   |---|---|---|
   | `GPIOx_MODER` | 0x00 | pin mode (input, output, ...) |
   | `GPIOx_IDR` | 0x10 | read pin levels |
   | `GPIOx_ODR` | 0x14 | output levels |
   | `GPIOx_BSRR` | 0x18 | set or reset single pins |

So `GPIOA_BSRR = 0x40020000 + 0x18 = 0x40020018`.

The Cortex-M4 core has its own peripherals (SysTick, NVIC). They are defined by Arm, not ST,
so they are documented in PM0214 instead of RM0368, at addresses starting with `0xE000`.

## From address to C
```c
#define REG32(addr) (*(volatile uint32_t *)(addr))
#define GPIOA_BSRR  REG32(0x40020018u)

GPIOA_BSRR = 0x20;   /* write 0x20 to the register at 0x40020018 */
```
Read it from the inside out: the number is cast to "pointer to volatile 32-bit unsigned",
then `*` dereferences it, so the whole macro behaves like a variable that lives at that
address.

### Why volatile
Peripheral registers change without your code touching them (a button is pressed, a flag
sets). If the compiler does not know that, it may optimise accesses away:

```c
while ((ms_ticks - start) < ms) { }   /* ms_ticks is changed by an interrupt */
```
Without `volatile` the compiler sees that the loop never writes `ms_ticks`, may read it once,
and turns the loop into an infinite one. `volatile` forces a real memory access every time.
Use it on every register and on every variable shared with an interrupt handler.

## Exercises
1. What is the address of `GPIOB_ODR`?
2. What is the address of `GPIOC_IDR`?
3. Look up `RCC_AHB1ENR` in RM0368 chapter 6. What is its offset, and which bit enables GPIOC?

<details><summary>Answers</summary>

1. `0x40020400 + 0x14 = 0x40020414`
2. `0x40020800 + 0x10 = 0x40020810`
3. Offset `0x30` (address `0x40023830`); GPIOCEN is bit 2.
</details>

Next: [Binary, hex and masks](02-binary-hex-and-masks.md)
