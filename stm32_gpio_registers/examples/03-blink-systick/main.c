/*
 * Example 03: blink LD2 at exactly 1 Hz using the SysTick timer.
 *
 * Same as example 01, but the delay comes from a hardware timer interrupt
 * (common/systick.c), so the timing does not depend on how the compiler
 * optimised a loop. Walkthrough: docs/06-systick-and-delays.md
 */
#include "stm32f401_regs.h"
#include "systick.h"

int main(void)
{
    systick_init();

    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA_MODER &= ~(3u << (LED_PIN * 2u));
    GPIOA_MODER |=  (1u << (LED_PIN * 2u));

    while (1) {
        GPIOA_BSRR = (1u << LED_PIN);
        delay_ms(500u);
        GPIOA_BSRR = (1u << (LED_PIN + 16u));
        delay_ms(500u);
    }
}
