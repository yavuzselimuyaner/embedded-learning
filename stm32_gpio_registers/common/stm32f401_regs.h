/*
 * Minimal register definitions for the STM32F401RE.
 *
 * Everything here comes from two documents:
 *   RM0368  STM32F401 reference manual   (RCC, GPIO)
 *   PM0214  STM32 Cortex-M4 programming manual (SysTick)
 *
 * Address = peripheral base address + register offset.
 * See docs/01-memory-mapped-registers.md for how to look these up yourself.
 */
#ifndef STM32F401_REGS_H
#define STM32F401_REGS_H

#include <stdint.h>

/* Treat a fixed address as a 32-bit register.
 * volatile: the hardware may change the value on its own, so the compiler
 * must not cache, merge or remove the accesses. */
#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ---- RCC: reset and clock control (RM0368 chapter 6) ---- */
#define RCC_BASE            0x40023800u
#define RCC_AHB1ENR         REG32(RCC_BASE + 0x30u)
#define RCC_AHB1ENR_GPIOAEN (1u << 0)
#define RCC_AHB1ENR_GPIOBEN (1u << 1)
#define RCC_AHB1ENR_GPIOCEN (1u << 2)

/* ---- GPIO (RM0368 chapter 8) ---- */
#define GPIOA_BASE 0x40020000u
#define GPIOB_BASE 0x40020400u
#define GPIOC_BASE 0x40020800u

#define GPIO_MODER_OFFSET 0x00u  /* mode, 2 bits per pin       */
#define GPIO_IDR_OFFSET   0x10u  /* input data, read-only      */
#define GPIO_ODR_OFFSET   0x14u  /* output data                */
#define GPIO_BSRR_OFFSET  0x18u  /* bit set/reset, write-only  */

#define GPIOA_MODER REG32(GPIOA_BASE + GPIO_MODER_OFFSET)
#define GPIOA_IDR   REG32(GPIOA_BASE + GPIO_IDR_OFFSET)
#define GPIOA_ODR   REG32(GPIOA_BASE + GPIO_ODR_OFFSET)
#define GPIOA_BSRR  REG32(GPIOA_BASE + GPIO_BSRR_OFFSET)

#define GPIOB_MODER REG32(GPIOB_BASE + GPIO_MODER_OFFSET)
#define GPIOB_IDR   REG32(GPIOB_BASE + GPIO_IDR_OFFSET)
#define GPIOB_ODR   REG32(GPIOB_BASE + GPIO_ODR_OFFSET)
#define GPIOB_BSRR  REG32(GPIOB_BASE + GPIO_BSRR_OFFSET)

#define GPIOC_MODER REG32(GPIOC_BASE + GPIO_MODER_OFFSET)
#define GPIOC_IDR   REG32(GPIOC_BASE + GPIO_IDR_OFFSET)
#define GPIOC_ODR   REG32(GPIOC_BASE + GPIO_ODR_OFFSET)
#define GPIOC_BSRR  REG32(GPIOC_BASE + GPIO_BSRR_OFFSET)

/* ---- SysTick: Cortex-M4 core timer (PM0214), not part of RM0368 ---- */
#define SYST_CSR REG32(0xE000E010u)  /* control and status */
#define SYST_RVR REG32(0xE000E014u)  /* reload value       */
#define SYST_CVR REG32(0xE000E018u)  /* current value      */

/* ---- Board wiring (UM1724) ---- */
#define LED_PIN     5u   /* LD2 user LED, port A */
#define BUTTON_PIN  13u  /* B1 user button, port C, reads 0 while pressed */

#endif
