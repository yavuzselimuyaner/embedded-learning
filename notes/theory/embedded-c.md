# Embedded C

C for microcontrollers is the same language you learned in class, used differently.
No OS underneath, no virtual memory, kilobytes instead of gigabytes, and code that has
to keep running for months without a restart. That changes which idioms are correct.

---

## 1. Use fixed-width types

`int` has no guaranteed size. On a PC it is usually 32 bits; on an 8-bit AVR it is 16.
Firmware that assumes a size and then moves to another chip breaks silently.

```c
#include <stdint.h>

uint8_t   flags;      // exactly 8 bits, unsigned
int16_t   temp_c;     // exactly 16 bits, signed
uint32_t  reg_value;  // exactly 32 bits, unsigned
```

Rules of thumb:

- Registers, buffers, protocol frames → **always** fixed-width types.
- Sizes and counts → `size_t`.
- Loop counters over small ranges → `int` is fine; do not micro-optimise readability away.
- A byte of raw data is `uint8_t`, not `char`. `char`'s signedness is implementation-defined.

`uintN_t` types come from `<stdint.h>`. `bool` comes from `<stdbool.h>`.

---

## 2. Bit manipulation

Registers are 32-bit boxes where individual bits mean different things. Four operations
cover almost everything:

```c
reg |=  (1u << n);    // set bit n
reg &= ~(1u << n);    // clear bit n
reg ^=  (1u << n);    // toggle bit n
if (reg & (1u << n))  // test bit n
```

Multiple bits at once use a **mask**:

```c
#define MODE_MASK   (0x3u << 4)     // bits 5:4
#define MODE_OUTPUT (0x1u << 4)

reg = (reg & ~MODE_MASK) | MODE_OUTPUT;   // clear the field, then write it
```

That last line is the standard "read-modify-write a bit field" idiom: clear the whole
field first, then OR in the new value. Skipping the clear leaves old bits set.

**Write `1u`, not `1`.** `1` is a signed int; shifting into the sign bit
(`1 << 31`) is undefined behaviour. `1u` is unsigned and well-defined.

Bit fields wider than 32 need `1ull`:

```c
.pin_bit_mask = (1ULL << BUTTON_GPIO)    // ESP-IDF uses a 64-bit pin mask
```

---

## 3. `volatile`

`volatile` tells the compiler: **this can change without your code changing it — never
cache it, never optimise the access away.**

Three cases where it is mandatory:

**Memory-mapped hardware registers.** The value changes because the peripheral changed it.

```c
volatile uint32_t *status = (volatile uint32_t *)0x60004000;
while ((*status & READY_BIT) == 0) { }   // without volatile: infinite loop
```

Without `volatile` the compiler reads the address once, sees the loop body is empty, and
turns it into `while(1)`.

**Variables shared between an ISR and normal code.**

```c
static volatile uint32_t isr_count;
```

The compiler cannot see that the ISR runs, so it assumes nothing modifies the variable.

**Variables shared between tasks** — though here `volatile` alone is not enough (see below).

### What `volatile` does NOT do

This trips up almost everyone:

- **It does not make access atomic.** `counter++` on a `volatile` variable is still
  read-modify-write and can still be interrupted halfway.
- **It is not a lock.** It solves visibility, not mutual exclusion.
- **It is not a memory barrier** for multi-core ordering.

For counters shared across contexts you need a critical section, an atomic type, or a
lock-free design (e.g. only the ISR writes, only the main loop reads).

---

## 4. `static` has two meanings

**At file scope:** the symbol is private to this `.c` file. Nothing else can link to it.

```c
static uint8_t s_led_state;      // invisible outside this file
static void blink_led(void);     // internal helper
```

Use it by default for everything not in a header. It keeps the namespace clean and lets
the compiler optimise more aggressively.

**Inside a function:** the variable survives between calls and lives in static memory,
not on the stack.

```c
static void IRAM_ATTR button_isr(void *arg)
{
    static int64_t last_us = 0;      // retains its value across interrupts
    ...
}
```

Same keyword, unrelated meanings. Read it by position.

---

## 5. `const` and where it binds

```c
const uint8_t *p;        // pointer to const data — cannot write *p, can move p
uint8_t *const p;        // const pointer — can write *p, cannot move p
const uint8_t *const p;  // both
```

Read right to left from the variable name.

`const` data can be placed in flash instead of RAM — meaningful when RAM is 300 KB and
flash is 16 MB. Lookup tables, font data, string constants: mark them `const`.

---

## 6. Accessing registers: pointers vs structs

**Pointer style** — explicit, always works:

```c
#define GPIO_OUT_REG  (*(volatile uint32_t *)0x60004004)
GPIO_OUT_REG |= (1u << 5);
```

**Struct style** — the vendor lays out a struct matching the register block, so the
compiler computes offsets for you:

```c
typedef struct {
    volatile uint32_t out;
    volatile uint32_t out_w1ts;
    volatile uint32_t out_w1tc;
} gpio_dev_t;

#define GPIO ((gpio_dev_t *)0x60004000)
GPIO->out_w1ts = (1u << 5);
```

ESP-IDF ships both (`soc/gpio_reg.h` and `soc/gpio_struct.h`); ST does the same for STM32.
Both map onto the register table in the Technical Reference Manual.

### Bit fields — use with care

```c
typedef struct {
    volatile uint32_t enable : 1;
    volatile uint32_t mode   : 2;
    volatile uint32_t        : 29;
} ctrl_reg_t;
```

Readable, but **bit packing order is implementation-defined** — a compiler may lay them
out from the other end. Vendor headers use them safely because they are written for one
compiler. In portable code, prefer masks and shifts.

Also: writing one bit field member is a read-modify-write of the whole register. On
hardware with write-1-to-clear bits, that can clear flags you never touched.

---

## 7. Unions for reinterpreting bytes

```c
typedef union {
    uint32_t word;
    uint8_t  bytes[4];
} reg_u;
```

Useful for splitting a value into bytes for a protocol frame. Beware **endianness** —
which byte comes out first depends on the architecture (ESP32 and ARM Cortex-M are
little-endian; many network protocols are big-endian).

For protocol code, explicit shifts are safer than unions:

```c
buf[0] = (uint8_t)(value >> 8);    // high byte first, unambiguous
buf[1] = (uint8_t)(value & 0xFF);
```

---

## 8. Callbacks and function pointers

Peripheral drivers call *your* function when something happens. That is a function
pointer.

```c
typedef void (*event_cb_t)(void *arg);

static void my_handler(void *arg) { ... }

const esp_timer_create_args_t args = {
    .callback = &my_handler,     // function pointer
    .name     = "led"
};
```

Reading the declaration: `void (*event_cb_t)(void *arg)` is "pointer to a function taking
`void*` and returning `void`". The `void *arg` is the standard way to pass context to a
callback in C, since there is no closure.

This pattern is everywhere in embedded APIs — timers, ISRs, DMA completion, protocol
stacks.

---

## 9. Dynamic memory: avoid it

`malloc` is discouraged, sometimes forbidden, in embedded code:

- **Fragmentation.** Repeated alloc/free on a 300 KB heap eventually fails even though
  total free memory looks fine. There is no OS to compact it, and the device is expected
  to run for months.
- **Non-determinism.** `malloc` has no bounded execution time — unacceptable in a
  real-time path.
- **Silent failure.** Out of memory in a firmware image with no user to tell.

Alternatives:

```c
static uint8_t rx_buffer[256];        // static allocation, size known at compile time
```

- Static buffers sized for the worst case
- Fixed-size pools of pre-allocated objects
- Ring buffers for streams

If you must allocate, do it **once at startup** and never free. MISRA C and most
safety-critical standards forbid dynamic allocation after initialisation outright.

---

## 10. Integer pitfalls

**Signed/unsigned comparison.** The signed value gets converted to unsigned:

```c
int      i = -1;
unsigned u = 1;
if (i < u) { }      // FALSE — i becomes a huge unsigned number
```

Compile with `-Wall -Wextra` and the compiler warns about this.

**Integer promotion.** Anything smaller than `int` gets promoted to `int` in expressions:

```c
uint8_t a = 200, b = 100;
uint8_t c = a + b;          // computed as int (300), then truncated to 44
```

**Overflow.** Signed overflow is undefined behaviour; unsigned wraps around predictably.
For counters and timestamps, prefer unsigned — the wraparound subtraction still works:

```c
uint32_t elapsed = now - start;    // correct even when `now` has wrapped
```

**Shifting.** `x << n` where `n >= width of x` is undefined. Shifting a signed negative
value is undefined.

---

## 11. `sizeof`, alignment, padding

The compiler inserts padding so members land on their natural alignment:

```c
struct bad  { uint8_t a; uint32_t b; uint8_t c; };   // likely 12 bytes
struct good { uint32_t b; uint8_t a; uint8_t c; };   // likely 8 bytes
```

Ordering members from largest to smallest saves RAM. It matters when you have hundreds of
kilobytes, not gigabytes.

Never assume a struct's memory layout matches a protocol frame. Serialise field by field
instead of casting a struct onto a byte buffer — padding and endianness will bite you.

Unaligned access is slow on some architectures and a hard fault on others.

---

## 12. Macros vs inline functions

```c
#define SQUARE(x) ((x) * (x))       // SQUARE(i++) evaluates i++ twice
static inline int square(int x) { return x * x; }   // type-checked, evaluates once
```

Prefer `static inline`. Reserve macros for things functions cannot do: compile-time
constants, conditional compilation, register definitions, `#include` guards.

If you must write a function-like macro, wrap every parameter and the whole body in
parentheses.

---

## 13. The ring buffer

The single most-used data structure in firmware. A fixed array with a write index and a
read index, both wrapping around. Used whenever data arrives faster or less predictably
than you can process it — UART receive, ADC samples, log output.

```c
#define RB_SIZE 64                     // power of two makes wrapping a bitwise AND

typedef struct {
    uint8_t  buf[RB_SIZE];
    volatile uint16_t head;            // written by producer (often an ISR)
    volatile uint16_t tail;            // written by consumer
} ringbuf_t;
```

Key property: with **one producer and one consumer**, and only the producer touching
`head` and only the consumer touching `tail`, no lock is needed. That is why it fits the
ISR-to-main-loop pattern so well.

Worth implementing from scratch once, and it is a common interview exercise. It is also
testable on a PC with no hardware at all — good practice for unit testing.

---

## 14. Compiler flags worth knowing

| Flag | Effect |
|---|---|
| `-Wall -Wextra` | the warnings that catch real bugs |
| `-Werror` | treat warnings as errors — use it in CI |
| `-O0` | no optimisation, easiest to debug |
| `-Og` | optimise but keep debuggability |
| `-Os` | optimise for size — common default in firmware |
| `-O2` | optimise for speed; exposes missing `volatile` |

A bug that appears at `-O2` and vanishes at `-O0` is almost always a missing `volatile`,
undefined behaviour, or a race condition. The optimiser did not break your code; it
revealed that the code was already wrong.

---

## Practice without hardware

All of this is testable on a PC with `gcc`:

- Write a ring buffer and unit-test it
- Write bit-manipulation helpers (set/clear/extract a field) and test edge cases
- Write a small state machine with function pointers
- Take a struct, print `sizeof` before and after reordering members
- Write a parser for a fake protocol frame — length, payload, checksum
