#include "systick.h"
#include "stm32f401_regs.h"

/* Written by the interrupt handler, read by main code: must be volatile. */
static volatile uint32_t ms_ticks;

void SysTick_Handler(void)
{
    ms_ticks++;
}

void systick_init(void)
{
    SYST_RVR = 16000u - 1u;  /* 16 MHz / 16000 = 1 kHz, i.e. every 1 ms */
    SYST_CVR = 0u;           /* any write clears the current value      */
    SYST_CSR = 0x7u;         /* bit 0 enable, bit 1 interrupt, bit 2 core clock */
}

uint32_t millis(void)
{
    return ms_ticks;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;
    /* Unsigned subtraction gives the right elapsed time even if the
     * counter wrapped around between start and now. */
    while ((ms_ticks - start) < ms) { }
}
