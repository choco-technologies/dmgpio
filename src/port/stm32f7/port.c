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
 *  Weak EXTI ISR stubs – override in application code if needed.
 *
 *  Each handler passes a bitmask of the EXTI lines it services to the
 *  common handler which reads the EXTI PR register and dispatches to the
 *  registered dmgpio interrupt handler.
 * ====================================================================== */

__attribute__((weak)) void EXTI0_IRQHandler(void)
{
    stm32_gpio_exti_irq_handler(0x0001UL);
}

__attribute__((weak)) void EXTI1_IRQHandler(void)
{
    stm32_gpio_exti_irq_handler(0x0002UL);
}

__attribute__((weak)) void EXTI2_IRQHandler(void)
{
    stm32_gpio_exti_irq_handler(0x0004UL);
}

__attribute__((weak)) void EXTI3_IRQHandler(void)
{
    stm32_gpio_exti_irq_handler(0x0008UL);
}

__attribute__((weak)) void EXTI4_IRQHandler(void)
{
    stm32_gpio_exti_irq_handler(0x0010UL);
}

__attribute__((weak)) void EXTI9_5_IRQHandler(void)
{
    stm32_gpio_exti_irq_handler(0x03E0UL);
}

__attribute__((weak)) void EXTI15_10_IRQHandler(void)
{
    stm32_gpio_exti_irq_handler(0xFC00UL);
}
