#define DMOD_ENABLE_REGISTRATION    ON
#include "dmod.h"
#include "dmgpio_port.h"
#include "../stm32_common/stm32_common.h"


/**
 * @brief Initialize the DMDRVI module
 * 
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMDRVI interface module initialized (STM32F7)\n");
    return 0;
}

/**
 * @brief Deinitialize the DMDRVI module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    Dmod_Printf("DMDRVI interface module deinitialized (STM32F7)\n");
    return 0;
}

/* ======================================================================
 *  EXTI ISR handlers registered with the DMOD IRQ dispatcher.
 *
 *  Each handler passes a bitmask of the EXTI lines it services to the
 *  common handler which reads the EXTI PR register and dispatches to the
 *  registered dmgpio interrupt handler.
 * ====================================================================== */

DMOD_IRQ_HANDLER(EXTI0_IRQHandler)
{
    stm32_gpio_exti_irq_handler(0x0001UL);
}

DMOD_IRQ_HANDLER(EXTI1_IRQHandler)
{
    stm32_gpio_exti_irq_handler(0x0002UL);
}

DMOD_IRQ_HANDLER(EXTI2_IRQHandler)
{
    stm32_gpio_exti_irq_handler(0x0004UL);
}

DMOD_IRQ_HANDLER(EXTI3_IRQHandler)
{
    stm32_gpio_exti_irq_handler(0x0008UL);
}

DMOD_IRQ_HANDLER(EXTI4_IRQHandler)
{
    stm32_gpio_exti_irq_handler(0x0010UL);
}

DMOD_IRQ_HANDLER(EXTI9_5_IRQHandler)
{
    stm32_gpio_exti_irq_handler(0x03E0UL);
}

DMOD_IRQ_HANDLER(EXTI15_10_IRQHandler)
{
    stm32_gpio_exti_irq_handler(0xFC00UL);
}

