#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/* Start the Cortex-M SysTick timer: one interrupt per millisecond
 * (assumes the 16 MHz HSI clock the chip runs from after reset). */
void systick_init(void);

/* Milliseconds since systick_init(). Wraps after about 49.7 days. */
uint32_t millis(void);

/* Block for at least `ms` milliseconds. Safe across counter wrap-around. */
void delay_ms(uint32_t ms);

#endif
