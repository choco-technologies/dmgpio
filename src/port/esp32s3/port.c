#define DMOD_ENABLE_REGISTRATION    ON
#include "dmod.h"
#include "dmgpio_port.h"

/**
 * @brief Initialize the dmgpio port module (ESP32-S3)
 *
 * The GPIO matrix and the IO MUX are always clocked, there is nothing to enable here.
 * The GPIO interrupts are not supported yet (the dispatcher of the interrupts of the Xtensa ports is not there).
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMGPIO port module initialized (ESP32-S3)\n");
    return 0;
}

int dmod_deinit(void)
{
    Dmod_Printf("DMGPIO port module deinitialized (ESP32-S3)\n");
    return 0;
}
