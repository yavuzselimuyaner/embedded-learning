# Interrupts and concurrency

An embedded program is never really single-threaded. Even without an RTOS, interrupts
mean code can be suspended between any two machine instructions and something else can
run. Most hard firmware bugs live here.

---

## 1. What an interrupt is

A peripheral raises a signal. The CPU finishes the current instruction, saves enough
state to resume, jumps to a handler, runs it, restores state, and continues as if nothing
happened.

The handler is an **ISR** (Interrupt Service Routine).

The address of each handler lives in the **vector table**, an array of function pointers
at a known location. Interrupt number *n* fires → CPU reads entry *n* → jumps there.
On ARM Cortex-M the table sits at the start of flash by default (which is why a
bootloader must relocate it via `SCB->VTOR` before jumping to the application).

### Polling vs interrupts

```c
while (1) {                            // polling
    if (gpio_get_level(BUTTON) == 0) { handle(); }
}
```

- CPU is busy 100 % of the time
- Anything happening between two checks is missed
- Response time depends on the loop's worst case

```c
// interrupt
gpio_isr_handler_add(BUTTON, button_isr, NULL);
```

- CPU is free (can even sleep, which matters on battery)
- Nothing is missed — the hardware latches the event
- Response time is bounded and short

Polling is not always wrong. For a signal you must sample at an exactly fixed rate, or in
a tight loop where the event is expected immediately, polling is simpler and more
predictable. But "check if something happened" is almost always an interrupt.

---

## 2. Latency and jitter

- **Latency** — time from the event to the first instruction of your ISR. Made up of:
  finishing the current instruction, saving context, reading the vector, jumping.
- **Jitter** — variation in that latency. For control loops and signal sampling, jitter
  often matters more than average latency.

What increases both:

- Long ISRs elsewhere (if interrupts are not nested)
- **Critical sections** that disable interrupts
- Flash cache misses, if the ISR code is not in RAM
- A higher-priority interrupt running first

This is why the `IRAM_ATTR` rule exists on ESP32: code normally executes from flash
through a cache. If an interrupt arrives while the cache is being refilled and the ISR
itself lives in flash, the chip cannot fetch it. `IRAM_ATTR` places the function in
internal RAM, always reachable. Everything the ISR calls must also be in IRAM.

---

## 3. ISR rules

**Keep it short.** Normal code is stopped while the ISR runs. Everything else — other
interrupts of equal or lower priority, all tasks — waits.

**Never in an ISR:**

| Forbidden | Why |
|---|---|
| Logging / `printf` | slow, and often takes a lock |
| `malloc` / `free` | takes a lock, unbounded time |
| Blocking calls, `delay` | there is nothing to yield to |
| Long loops | starves everything else |
| Normal RTOS API calls | must use the `...FromISR` variants |
| Floating point (on some MCUs) | FPU context may not be saved |

**The pattern:** the ISR records that something happened; the main loop or a task does the
work.

```c
static volatile uint32_t s_isr_count = 0;

static void IRAM_ATTR button_isr(void *arg)
{
    s_isr_count++;            // that is all
}
```

---

## 4. Atomicity — the core problem

```c
counter++;
```

Looks like one operation. On the CPU it is three:

```
load  counter -> register
add   1
store register -> counter
```

An interrupt between the load and the store means the ISR's update is overwritten. This
is a **read-modify-write hazard** and it is the root of most concurrency bugs.

An operation that cannot be interrupted partway is **atomic**.

### Hardware sometimes gives you atomicity for free

This is why GPIO peripherals have separate set and clear registers:

```c
GPIO.out |= (1 << 5);              // read-modify-write — NOT atomic
GPIO.out_w1ts = (1 << 5);          // single write — atomic
```

`W1TS` = write 1 to set. Bits written as 1 are set, bits written as 0 are untouched. One
store instruction, nothing to interrupt. `W1TC` does the same for clearing.

Almost every modern MCU provides set/clear/toggle registers for exactly this reason. ARM
Cortex-M additionally offers *bit-banding* on some parts, which aliases individual bits
to their own word addresses so a single write affects one bit.

### `volatile` is not atomicity

`volatile` guarantees the compiler will not cache or reorder the access. It says nothing
about whether the operation can be interrupted. A `volatile uint32_t counter` is still
unsafe to `++` from two contexts.

Also: on a 32-bit CPU, reading a `uint64_t` takes two loads. It can tear — you can read
the low half from before an update and the high half from after.

---

## 5. Critical sections

The blunt fix: prevent interrupts for the few instructions that must be indivisible.

```c
portENTER_CRITICAL(&spinlock);
shared_counter++;
portEXIT_CRITICAL(&spinlock);
```

(Bare metal: disable/enable interrupts. ARM: `__disable_irq()` / `__enable_irq()`, or
better, save and restore the previous state.)

Rules:

- **As short as possible.** Every cycle inside adds to worst-case interrupt latency.
- **Never block inside one.** No delays, no waiting on anything.
- **Save and restore state** rather than unconditionally re-enabling — otherwise you
  enable interrupts inside a caller that had deliberately disabled them.

On multi-core parts (ESP32-S3 has two), disabling interrupts stops one core only. You
also need a spinlock — which is what the ESP-IDF macros above take as an argument.

### Lock-free alternatives

Often better than locking:

- **Single producer, single consumer ring buffer.** If only the ISR moves `head` and only
  the consumer moves `tail`, no lock is needed.
- **Flag instead of counter.** A `volatile bool` written only by the ISR and cleared only
  by the main loop has no read-modify-write conflict.
- **Atomic types** (`stdatomic.h`) where the architecture supports them.

---

## 6. Race conditions

A race is when the result depends on timing between two contexts. They are hard because
they are intermittent — the code works for hours, then fails once.

Classic shapes:

**Check-then-act**

```c
if (buffer_has_space()) {     // an ISR fills the buffer right here
    buffer_write(x);          // now it is full
}
```

**Lost update** — the `counter++` case above.

**Torn read** — reading a multi-word value while it is being updated.

**Use-after-disable** — a callback fires after you thought you had cancelled it.

Rules of thumb:

- Write down, for every shared variable, **which context writes it and which reads it**.
  Most races are visible in that table alone.
- Prefer one writer. Shared *mutable* state between contexts is what you are trying to
  minimise.
- Symptoms that suggest a race: works at `-O0` and fails at `-O2`; fails only under load;
  fails only when a debugger is *not* attached; fails once a day.

---

## 7. Getting data from an ISR to a task

Four patterns, in increasing capability:

| Pattern | Use when |
|---|---|
| **`volatile` flag** | "something happened", no data, losing repeats is fine |
| **Counter** | you need to know how many, and increments are made safe |
| **Ring buffer** | a stream of bytes — UART receive, ADC samples |
| **Queue / semaphore / task notification** | you have an RTOS and want a task to wake immediately |

With FreeRTOS, ISRs must use the `...FromISR` variants:

```c
static void IRAM_ATTR uart_isr(void *arg)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(queue, &byte, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}
```

That `higher_priority_task_woken` mechanism exists so the scheduler can switch directly to
the task you just unblocked, instead of waiting for the next tick. Ignoring it does not
break correctness but adds latency.

---

## 8. Priority, nesting, preemption

Interrupts have priorities. A higher-priority interrupt can preempt a lower-priority ISR
that is already running — **nesting**.

Consequences:

- A low-priority ISR must not assume it runs to completion undisturbed.
- Priority assignment is a design decision: the most timing-critical source (a motor
  control loop, a communication bus that will overflow) gets the highest priority. A
  button gets a low one.
- Total worst-case latency for a given interrupt = its own execution time + all
  higher-priority ISRs that can fire during it + the longest critical section anywhere in
  the system.

That last line is why long critical sections are a design smell: they set a floor under
every deadline in the system.

---

## 9. Debouncing

A mechanical switch does not close cleanly. The contacts bounce for typically 1–20 ms,
producing many edges from one press. An interrupt-driven input sees all of them.

**Time-based rejection** — ignore edges too close together:

```c
static void IRAM_ATTR button_isr(void *arg)
{
    static int64_t last_us = 0;
    int64_t now = esp_timer_get_time();

    if (now - last_us < 50000) {   // 50 ms
        return;
    }
    last_us = now;

    s_isr_count++;
}
```

**Timer-based confirmation** — on the first edge, disable the interrupt and start a
one-shot timer; when it expires, read the pin and re-enable. More robust, more code.

**Hardware** — an RC filter, or a Schmitt-trigger input. Costs parts, uses no CPU.

Choosing the window is engineering judgement: too short and bounce gets through, too long
and fast presses are swallowed. 20–50 ms is typical for human input.

The general lesson matters more than the technique: **physical inputs are noisy, and the
software has to know it.** The same applies to sensor readings, ADC samples, and every
edge coming off a wire.

---

## 10. RTOS concurrency — the same problems, one level up

If you have studied operating systems, the mapping is direct:

| OS course | FreeRTOS |
|---|---|
| Process / thread | Task |
| Scheduler | Scheduler (preemptive, priority based) |
| Context switch | Context switch |
| Mutual exclusion | Mutex |
| Semaphore | Semaphore (binary and counting) |
| IPC / message passing | Queue |
| Deadlock | Deadlock — same four conditions |
| Priority inversion | Priority inversion, solved by priority inheritance |

**Priority inversion**, since it comes up in interviews and in the Mars Pathfinder story:
a low-priority task holds a mutex; a high-priority task blocks waiting on it; a
medium-priority task preempts the low-priority one. The high-priority task is now
effectively blocked by the medium one. **Priority inheritance** fixes it — while a
low-priority task holds a mutex wanted by a high-priority task, it temporarily inherits
that higher priority.

Note the distinction: use a **mutex** for protecting a resource (it supports priority
inheritance), and a **semaphore** for signalling between contexts (it does not).

---

## 11. Questions worth being able to answer

- What happens, step by step, when an interrupt fires?
- Why must an ISR be short? What are the concrete consequences of a long one?
- Why is `counter++` unsafe between an ISR and main code, and what are three ways to fix it?
- What does `volatile` guarantee, and what does it *not* guarantee?
- Why do GPIO peripherals have separate set and clear registers?
- What is a critical section and what is the cost of a long one?
- How would you get 1000 bytes/second from a UART ISR to a processing task?
- Why does a single button press produce several interrupts, and how do you fix it?
- What is priority inversion and how is it solved?
- A bug appears at `-O2` but not `-O0`. What are your first three hypotheses?
