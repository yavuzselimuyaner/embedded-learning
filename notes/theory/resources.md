# Resources

Curated, not exhaustive. Ordered by **when** to use them, not by topic. The full
roadmap lists hundreds of links; reading a list is not learning. Pick one thing per
stage and finish it.

Background this is tailored to: computer engineering graduate, strong on C / data
structures / OS / computer architecture, **no electronics course**, currently on ESP32-S3
with ESP-IDF, STM32 planned later, job market Turkey.

---

## If you read only one book

**Elecia White — *Making Embedded Systems: Design Patterns for Great Software*** (O'Reilly)

Written for exactly your situation: a software person entering embedded. Covers how
firmware is *structured* — state machines, interrupt design, memory constraints, hardware
abstraction, debugging — rather than one vendor's API. Readable, opinionated, no
electronics prerequisite. A 2nd edition exists.

Start it now and read it alongside the practical work. Almost everything in it will map
onto something you are building that week.

---

## Right now (Blocks 1–4: peripherals and protocols)

**Espressif's own documentation** — the ESP-IDF Programming Guide and the ESP32-S3
Technical Reference Manual. Not optional and not a fallback: the examples in
`esp-idf/examples/` are guaranteed to match your version, which blog posts are not.
Learning to search a TRM is itself one of the core skills.

**Interrupt blog (Memfault)** — `interrupt.memfault.com`. The best embedded engineering
blog that exists. Written by working firmware engineers, deep and specific. Relevant now:

- *I2C in a Nutshell*
- *A Guide to Watchdog Timers for Embedded Systems*
- *The Best and Worst GCC Compiler Flags For Embedded*
- *Embedded C/C++ Unit Testing Basics*
- *From Zero to main()* series — bare-metal startup, linker scripts, bootloaders

Read individual articles as the topic comes up, not front to back.

**Vedat Ozan Öner — *Developing IoT Projects with ESP32*** (Packt) — the one book that
matches your exact board and framework. Useful if you want a structured path through
ESP-IDF rather than assembling it from examples.

**Nordic Developer Academy** — `academy.nordicsemi.com`. Free, structured, genuinely
well-produced courses. Nordic hardware, but the Bluetooth LE and cellular IoT
fundamentals transfer directly.

---

## Video, free, worth the time

**Miro Samek — *Modern Embedded Systems Programming*** (YouTube). The single best free
embedded course online. Starts at the assembly and register level on ARM Cortex-M and
builds upward to RTOS concepts. Slow, precise, no hand-waving. Uses STM32, so it doubles
as preparation for your STM32 phase.

**Shawn Hymel / Digi-Key — *Introduction to RTOS*** (YouTube playlist). The clearest
introduction to FreeRTOS anywhere. Tasks, queues, semaphores, mutexes, priority inversion,
one concept per episode. Watch when you reach Block 6 — with your OS course background
you will move through it fast.

**Ben Eater** (YouTube). Builds an 8-bit computer on breadboards, and has a networking
series. Not directly applicable to daily firmware work, but nothing else explains what is
physically happening as well.

**Low Byte Productions** (YouTube) — bare-metal ARM from scratch, long form. Good after
you start STM32.

**Jacob Sorber** (YouTube) — short, sharp videos on C and systems topics.

---

## When you reach RTOS (Block 6)

***Mastering the FreeRTOS Real Time Kernel*** — the official FreeRTOS book, **free PDF**
from freertos.org. This is the reference, written by the people who wrote the kernel.
Start here before any third-party material.

**Brian Amos — *Hands-On RTOS with Microcontrollers*** (Packt) — practical FreeRTOS on
STM32 with real debugging tooling. Good complement.

Your operating systems course is a real advantage here. Task/process, scheduler,
mutex/mutual exclusion, queue/IPC, deadlock, priority inversion — the vocabulary is the
same, the difference is that it now runs on 300 KB of RAM.

---

## When you move to STM32

**Carmine Noviello — *Mastering STM32*** (Leanpub) — the standard practical STM32 book,
kept updated. Covers the toolchain, HAL, and peripherals thoroughly.

**Fastbit Embedded Brain Academy** (Udemy, Kiran Nayak) — the *Microcontroller Embedded C
Programming* and *STM32 driver development* courses. You write peripheral drivers from
scratch against the reference manual instead of calling HAL. That is exactly the skill
Turkish defence and automotive interviews probe. Paid, but Udemy discounts are constant —
never pay list price.

**Joseph Yiu — *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*** —
reference, not a read-through. For when you need to know precisely how NVIC, SysTick,
fault handling, or the memory model work.

**Daniele Lacamera — *Embedded Systems Architecture*** (Packt) — bare-metal ARM, boot
process, memory layout, secure boot. Good once the basics are solid.

---

## Your actual gap: electronics

You never took the electronics course. You do not need to become a hardware designer, but
you need enough to not be helpless in front of a circuit.

### Start here if the basics are genuinely new

If questions like *why do we need ground?*, *why does an LED need a resistor?*, *why is
one LED leg longer?* do not have instant answers yet, start with a real beginner book
rather than a reference.

**Charles Platt — *Make: Electronics*** — the best first electronics book for someone who
learns by building. Every concept arrives through an experiment you actually perform:
short a battery and watch a fuse blow, burn out an LED on purpose to see why the resistor
matters, build a circuit and then measure it. You already own a breadboard, resistors,
LEDs and buttons, so most of the early chapters are doable tonight. It assumes zero
background and never hides behind formulas.

**Forrest Mims — *Getting Started in Electronics*** — a hand-drawn pocket book, cheap and
about 130 pages. Covers current, voltage, resistance, Ohm''s law, diodes, transistors and
basic circuits with no fluff. Written in the 1980s and still one of the clearest
introductions ever produced. Good as a quick reference to keep on the desk.

**All About Circuits — *Lessons in Electric Circuits*** — free, online, complete
multi-volume textbook. Volume I (DC) covers exactly the fundamentals above. Use it when
you want the proper explanation of something a beginner book skimmed.

Then move on to:

**build-electronic-circuits.com** and **electronics-tutorials.ws** — free, practical, aimed
at exactly the "I write software but need to understand this circuit" level. Start here.

**Paul Scherz & Simon Monk — *Practical Electronics for Inventors*** — the best single
book for a software person. Broad, practical, not a university textbook.

**Charles Platt — *Make: Electronics*** — learn-by-doing, gentler, built around
experiments. Good if you prefer building to reading.

**Horowitz & Hill — *The Art of Electronics*** — the canonical reference. **Do not read it
cover to cover.** Look things up in it. Buying it as a beginner and trying to work through
it is a classic way to lose two months.

Target: Ohm's law, voltage dividers, pull-ups, open-drain vs push-pull, decoupling
capacitors, current limiting, transistor as a switch, level shifting, and reading the
electrical characteristics table of a datasheet. That is roughly 10–15 hours of material,
and it covers most of what firmware work demands.

---

## Testing and code quality (Block 7)

**James Grenning — *Test Driven Development for Embedded C*** (Pragmatic Bookshelf) — the
book on this subject. How to test firmware logic on a PC, how to mock hardware, how to
structure code so it is testable at all. Read it before your final project, not after.

**Barr Group's *Embedded C Coding Standard*** — free, short, opinionated. Worth one read
for the reasoning behind each rule.

---

## Reference — look things up, do not read straight through

- **The Art of Electronics** — Horowitz & Hill
- **The Definitive Guide to ARM Cortex-M** — Joseph Yiu
- **K&R, *The C Programming Language*** — short enough to actually finish, and still the
  best description of the language. Worth a weekend even though you know C.
- **Your chip's TRM and datasheet** — the ones you will use most often

---

## Skip for now

Not because they are bad — because they are the wrong stage:

- **Yocto / Buildroot / embedded Linux books** — a different discipline; only after MCU
  work is solid
- **AUTOSAR** — only if you go automotive, and normally learned on the job
- **PCB design courses (Altium, KiCad)** — hardware engineer's track
- **DSP textbooks** — needed for signal-processing roles, not for general firmware
- **Computer architecture textbooks** — you already have this, with a good grade

---

## How to actually use this

One resource at a time, alongside building something. The failure mode is collecting
material instead of finishing any of it.

A workable order:

1. *Making Embedded Systems* — start now, read a chapter a week
2. Electronics basics from the two free sites — 10–15 hours total, spread out
3. Interrupt blog articles as each topic comes up
4. Miro Samek's YouTube course during the STM32 phase
5. The FreeRTOS book at Block 6
6. Grenning's TDD book before the final project

Six things over several months. That is enough.
