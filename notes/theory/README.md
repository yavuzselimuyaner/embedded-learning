# Theory notes

Hardware-independent material. Readable anywhere, no board required.

| File | Contents |
|---|---|
| [embedded-c.md](embedded-c.md) | Fixed-width types, bit manipulation, `volatile`, `static`, register access patterns, callbacks, why `malloc` is avoided, integer pitfalls, ring buffers, compiler flags |
| [protocols.md](protocols.md) | UART, I2C, SPI — framing, wiring, failure modes, how to choose, and the interview questions that follow |
| [resources.md](resources.md) | Curated books, courses and blogs, ordered by when to use them |
| [interrupts-and-concurrency.md](interrupts-and-concurrency.md) | Interrupt mechanics, latency, ISR rules, atomicity, race conditions, critical sections, ISR-to-task patterns, debouncing, RTOS mapping |

Each file ends with questions to self-test against.

## Practising without hardware

Most of `embedded-c.md` compiles with plain `gcc` on a PC:

- implement a ring buffer and unit-test it
- write bit-field set/clear/extract helpers and test the edge cases
- build a small state machine using function pointers
- write a parser for a made-up protocol frame (length, payload, checksum)
- print `sizeof` a struct before and after reordering its members
