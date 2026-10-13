/*
 * Example 01: blink the green LED (LD2, PA5) with the crudest possible delay.
 *
 * Steps: enable the GPIOA clock, make PA5 an output, then set and reset it
 * through BSRR. Walkthrough: docs/03-gpio-output-led.md
 */
#include "stm32f401_regs.h"

static void crude_delay(void)
{
    /* volatile stops the compiler from deleting the empty loop.
     * The duration depends on clock and optimisation level: it is only
     * "about half a second". Example 03 uses a real timer instead. */
    for (volatile uint32_t i = 0; i < 400000u; i++) { }
}

int main(void)
{
    /* 1. Clock first: GPIOA ignores register writes while its clock is off. */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* 2. Pin mode: two bits per pin at bit 2n. Clear the field, then write 01. */
    GPIOA_MODER &= ~(3u << (LED_PIN * 2u));
    GPIOA_MODER |=  (1u << (LED_PIN * 2u));    /* 01 = general purpose output */

    while (1) {
        GPIOA_BSRR = (1u << LED_PIN);          /* low half of BSRR: set pin   */
        crude_delay();
        GPIOA_BSRR = (1u << (LED_PIN + 16u));  /* high half: reset pin        */
        crude_delay();
    }
}
