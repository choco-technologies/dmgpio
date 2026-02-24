#ifndef DMGPIO_STM32_REGS_H
#define DMGPIO_STM32_REGS_H

#include <stdint.h>

/**
 * @brief STM32 GPIO register layout
 */
typedef struct
{
    volatile uint32_t MODER;    /**< GPIO port mode register */
    volatile uint32_t OTYPER;   /**< GPIO port output type register */
    volatile uint32_t OSPEEDR;  /**< GPIO port output speed register */
    volatile uint32_t PUPDR;    /**< GPIO port pull-up/pull-down register */
    volatile uint32_t IDR;      /**< GPIO port input data register */
    volatile uint32_t ODR;      /**< GPIO port output data register */
    volatile uint32_t BSRR;     /**< GPIO port bit set/reset register */
    volatile uint32_t LCKR;     /**< GPIO port configuration lock register */
    volatile uint32_t AFR[2];   /**< GPIO alternate function registers */
} GPIO_TypeDef;

/* GPIO MODER register bit fields */
#define GPIO_MODER_INPUT        0x00U   /**< Input mode */
#define GPIO_MODER_OUTPUT       0x01U   /**< Output mode */
#define GPIO_MODER_AF           0x02U   /**< Alternate function mode */
#define GPIO_MODER_ANALOG       0x03U   /**< Analog mode */

/* GPIO OTYPER register bit fields */
#define GPIO_OTYPER_PP          0x00U   /**< Push-pull output */
#define GPIO_OTYPER_OD          0x01U   /**< Open-drain output */

/* GPIO OSPEEDR register bit fields */
#define GPIO_OSPEEDR_LOW        0x00U   /**< Low speed */
#define GPIO_OSPEEDR_MEDIUM     0x01U   /**< Medium speed */
#define GPIO_OSPEEDR_HIGH       0x02U   /**< High speed */
#define GPIO_OSPEEDR_VERY_HIGH  0x03U   /**< Very high speed */

/* GPIO PUPDR register bit fields */
#define GPIO_PUPDR_NONE         0x00U   /**< No pull-up or pull-down */
#define GPIO_PUPDR_UP           0x01U   /**< Pull-up */
#define GPIO_PUPDR_DOWN         0x02U   /**< Pull-down */

/* GPIO IDR/ODR pin mask */
#define GPIO_PIN_MASK(pin)      (1U << (pin))

/* GPIO BSRR bit set/reset */
#define GPIO_BSRR_SET(pin)      (1U << (pin))
#define GPIO_BSRR_RESET(pin)    (1U << ((pin) + 16U))

/* SYSCFG register base (STM32F4/F7) */
#define SYSCFG_BASE             0x40013800U
#define SYSCFG_APB2ENR_BIT      14U   /**< SYSCFGEN bit in RCC APB2ENR */
#define RCC_APB2ENR             (*(volatile uint32_t *)(0x40023800U + 0x44U))

/* SYSCFG_EXTICRx - connect GPIO port to EXTI line */
#define SYSCFG_EXTICR(n)        (*(volatile uint32_t *)(SYSCFG_BASE + 0x08U + ((n) * 4U)))

/* EXTI registers */
#define EXTI_BASE               0x40013C00U
#define EXTI_IMR                (*(volatile uint32_t *)(EXTI_BASE + 0x00U))
#define EXTI_RTSR               (*(volatile uint32_t *)(EXTI_BASE + 0x08U))
#define EXTI_FTSR               (*(volatile uint32_t *)(EXTI_BASE + 0x0CU))
#define EXTI_PR                 (*(volatile uint32_t *)(EXTI_BASE + 0x14U))

#endif /* DMGPIO_STM32_REGS_H */
