/*
 * Startup code and vector table for the Cortex-M4 in the STM32F401RE.
 *
 * On reset the core reads two words from address 0x08000000 (flash):
 *   word 0: initial stack pointer
 *   word 1: address of the reset handler, which it then jumps to
 * The vector table below provides them, followed by the exception handlers.
 */
#include <stdint.h>

extern int main(void);

/* Symbols provided by the linker script. */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

void Reset_Handler(void)
{
    /* Initialised globals live in flash; copy them to RAM. */
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }
    /* Uninitialised and zero-initialised globals: clear them. */
    for (dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0;
    }
    main();
    while (1) { }
}

void Default_Handler(void) { while (1) { } }

/* Any handler an application does not define falls back to Default_Handler. */
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"), used))
static void (* const vectors[16])(void) = {
    (void (*)(void))&_estack,   /*  0 initial stack pointer */
    Reset_Handler,              /*  1 reset                 */
    Default_Handler,            /*  2 NMI                   */
    Default_Handler,            /*  3 HardFault             */
    Default_Handler,            /*  4 MemManage             */
    Default_Handler,            /*  5 BusFault              */
    Default_Handler,            /*  6 UsageFault            */
    0, 0, 0, 0,                 /*  7-10 reserved           */
    Default_Handler,            /* 11 SVCall                */
    Default_Handler,            /* 12 DebugMonitor          */
    0,                          /* 13 reserved              */
    Default_Handler,            /* 14 PendSV                */
    SysTick_Handler,            /* 15 SysTick               */
};
