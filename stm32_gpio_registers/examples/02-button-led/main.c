/*
 * Example 02: the LED follows the blue user button (B1, PC13).
 *
 * The button is active low: the pin reads 1 when released and 0 while
 * pressed. Walkthrough: docs/04-gpio-input-button.md
 */
#include "stm32f401_regs.h"

int main(void)
{
    /* The LED is on port A and the button on port C: two clocks. */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;

    GPIOA_MODER &= ~(3u << (LED_PIN * 2u));
    GPIOA_MODER |=  (1u << (LED_PIN * 2u));        /* PA5  = output */

    GPIOC_MODER &= ~(3u << (BUTTON_PIN * 2u));     /* PC13 = input (00) */

    while (1) {
        if ((GPIOC_IDR & (1u << BUTTON_PIN)) == 0u) {
            GPIOA_BSRR = (1u << LED_PIN);          /* pressed: LED on  */
        } else {
            GPIOA_BSRR = (1u << (LED_PIN + 16u));  /* released: LED off */
        }
    }
}
