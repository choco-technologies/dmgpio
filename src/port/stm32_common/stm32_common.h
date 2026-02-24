#ifndef STM32_COMMON_H
#define STM32_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include "dmgpio_port.h"

/**
 * @brief STM32 GPIO register layout (same for F4 and F7 families).
 */
typedef struct
{
    volatile uint32_t MODER;    /**< Mode register */
    volatile uint32_t OTYPER;   /**< Output type register */
    volatile uint32_t OSPEEDR;  /**< Output speed register */
    volatile uint32_t PUPDR;    /**< Pull-up/pull-down register */
    volatile uint32_t IDR;      /**< Input data register */
    volatile uint32_t ODR;      /**< Output data register */
    volatile uint32_t BSRR;     /**< Bit set/reset register */
    volatile uint32_t LCKR;     /**< Lock register */
    volatile uint32_t AFR[2];   /**< Alternate function registers */
} stm32_gpio_t;

/**
 * @brief STM32 EXTI register layout.
 */
typedef struct
{
    volatile uint32_t IMR;    /**< Interrupt mask register */
    volatile uint32_t EMR;    /**< Event mask register */
    volatile uint32_t RTSR;   /**< Rising trigger selection register */
    volatile uint32_t FTSR;   /**< Falling trigger selection register */
    volatile uint32_t SWIER;  /**< Software interrupt event register */
    volatile uint32_t PR;     /**< Pending register */
} stm32_exti_t;

/** GPIO port A base address */
#define STM32_GPIOA_BASE        0x40020000UL
/** Size of each GPIO port register block */
#define STM32_GPIO_PORT_SIZE    0x00000400UL
/** Pointer to the GPIO register block for a given port index (0=A, 1=B, ...) */
#define STM32_GPIO(port)        ((stm32_gpio_t *)(STM32_GPIOA_BASE + (uint32_t)(port) * STM32_GPIO_PORT_SIZE))

/** RCC AHB1 peripheral clock enable register */
#define STM32_RCC_AHB1ENR       (*(volatile uint32_t *)0x40023830UL)
/** SYSCFG external interrupt configuration registers (EXTICR1-4) */
#define STM32_SYSCFG_EXTICR     ((volatile uint32_t *)0x40013808UL)
/** EXTI controller base */
#define STM32_EXTI              ((stm32_exti_t *)0x40013C00UL)

/** NVIC Interrupt Set-Enable Registers */
#define STM32_NVIC_ISER         ((volatile uint32_t *)0xE000E100UL)
/** NVIC Interrupt Clear-Enable Registers */
#define STM32_NVIC_ICER         ((volatile uint32_t *)0xE000E180UL)

/** Maximum number of GPIO ports supported (A=0 … K=10) */
#define STM32_MAX_PORTS         11U

/**
 * @brief Handle a GPIO EXTI interrupt for the specified pending lines.
 *
 * Should be called from every EXTI ISR handler with a bitmask of the EXTI
 * lines serviced by that ISR (e.g. 0x0001 for EXTI0, 0x03E0 for EXTI9_5).
 *
 * @param exti_lines Bitmask of EXTI lines handled by the calling ISR.
 */
void stm32_gpio_exti_irq_handler(uint32_t exti_lines);

#endif // STM32_COMMON_H
