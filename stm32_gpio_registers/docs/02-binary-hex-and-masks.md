# 2. Binary, hex and masks

Register work is mostly one skill: change some bits of a 32-bit number and leave the others
alone. This chapter builds that skill from the ground up.

## Positional notation
In decimal each position is worth ten times the one to its right: `345 = 3×100 + 4×10 + 5`.
In binary each position is worth **two** times the one to its right:

```
bit:     7    6    5    4    3    2    1    0
weight: 128   64   32   16    8    4    2    1

0010 1101 = 32 + 8 + 4 + 1 = 45
```
Bits are numbered from the right, starting at **0**. A 32-bit register has bits 0 to 31.

## Hex is a shorthand for binary
One hex digit is exactly four bits, so you can convert without any arithmetic: split the
binary into groups of four from the right and look each group up.

| Bits | Hex | Bits | Hex |
|---|---|---|---|
| 0000 | 0 | 1000 | 8 |
| 0001 | 1 | 1001 | 9 |
| 0010 | 2 | 1010 | A |
| 0011 | 3 | 1011 | B |
| 0100 | 4 | 1100 | C |
| 0101 | 5 | 1101 | D |
| 0110 | 6 | 1110 | E |
| 0111 | 7 | 1111 | F |

```
0010 0000          →  0x20
1010 1000 0000 ... →  0xA8000000
```
`0x` is the C prefix for hex. Eight hex digits make a 32-bit value.

## Bitwise operators
| Operator | Name | Rule per bit |
|---|---|---|
| `a & b` | AND | 1 only if both are 1 |
| `a \| b` | OR | 1 if either is 1 |
| `a ^ b` | XOR | 1 if they differ |
| `~a` | NOT | flips every bit |
| `a << n` | shift left | moves bits left by n, fills with 0 |
| `a >> n` | shift right | moves bits right by n |

The two that matter most for registers:

- **AND with 0 clears a bit, AND with 1 keeps it.**
- **OR with 1 sets a bit, OR with 0 keeps it.**

So a **mask** is a number that has 1s where you want to act and 0s where you want to leave
things alone.

## Shifts build masks
The left operand is the value, the right operand is how far it moves:

| Expression | Binary | Hex |
|---|---|---|
| `1u << 0` | 0000 0001 | 0x01 |
| `1u << 3` | 0000 1000 | 0x08 |
| `1u << 5` | 0010 0000 | 0x20 |
| `1u << 7` | 1000 0000 | 0x80 |
| `5 << 1`  | 0000 1010 | 0x0A |

`1u << n` is a number with only bit n set. `5 << 1` and `1 << 5` are different expressions:
check which side is the value.

### Why the `u`
A plain `1` is a signed `int`. Shifting into the sign bit (`1 << 31`) is undefined behaviour in
C. `1u` is unsigned, so all 32 bits are well defined. Registers are bit patterns, not signed
numbers, so use unsigned everywhere.

## The four patterns
Given a register `reg` and bit number `n`:

| Goal | Code | What happens |
|---|---|---|
| Set bit n | `reg \|= (1u << n);` | OR with a mask that has only bit n |
| Clear bit n | `reg &= ~(1u << n);` | AND with a mask that has everything except bit n |
| Toggle bit n | `reg ^= (1u << n);` | XOR flips only bit n |
| Test bit n | `if (reg & (1u << n))` | non-zero result means the bit is set |

Walk through clear with `reg = 0b1111`, `n = 2`:
```
1u << 2        = 0100
~(1u << 2)     = ...1111 1011      (every bit except bit 2)
1111 & 1011    = 1011              bit 2 cleared, the rest untouched
```

## Fields wider than one bit
Some settings use several bits, for example a 2-bit mode. To write the value `v` into a field of
width `w` starting at bit `pos`:

```c
reg &= ~(((1u << w) - 1u) << pos);   /* 1. clear the whole field     */
reg |= (v << pos);                   /* 2. write the new value       */
```
Order matters. If you skip step 1 and the field currently holds `10`, then OR-ing in `01`
gives `11`, a completely different value. For a 2-bit field the mask `((1u << 2) - 1u)` is
`3u`.

## Why not just assign?
`reg = value;` replaces all 32 bits. If the register holds settings for 16 pins, you have just
overwritten the other 15. Masks are how you touch only what you mean to.

## Operator precedence trap
`+` binds tighter than `<<`, so `1u << pin + 16` means `1u << (pin + 16)`. That happens to be
what you want, but write the parentheses so the compiler (and readers) do not have to guess:
`1u << (pin + 16u)`.

## Exercises
1. Write `0xC0` in binary and `0b0001 0110` in hex.
2. Write one expression that clears bits 3 and 4 of `x` and leaves the rest alone.
3. Write an expression that is true when bit 9 of `x` is set.
4. Write a function `set_field(reg, pos, width, value)` and test it on the host with `assert`.
   Check that a field already holding `10` ends up with `01` after you write `01`.

<details><summary>Answers</summary>

1. `0xC0` = `1100 0000`; `0001 0110` = `0x16`.
2. `x &= ~((1u << 3) | (1u << 4));` (or `x &= ~(3u << 3);`)
3. `(x & (1u << 9)) != 0u`
4. 
```c
uint32_t set_field(uint32_t reg, unsigned pos, unsigned width, uint32_t value)
{
    uint32_t mask = ((width >= 32u) ? 0xFFFFFFFFu : ((1u << width) - 1u)) << pos;
    return (reg & ~mask) | ((value << pos) & mask);
}
```
</details>

Next: [GPIO output: the LED](03-gpio-output-led.md)
