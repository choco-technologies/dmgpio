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
    Dmod_Printf("DMGPIO Port module initialized (STM32F4)\n");
    return 0;
}

/**
 * @brief Deinitialize the DMDRVI module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    Dmod_Printf("DMGPIO Port module deinitialized (STM32F4)\n");
    return 0;
}
