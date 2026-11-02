#define DMOD_ENABLE_REGISTRATION    ON
#include "dmod.h"
#include "dmgpio_port.h"
#include "esp32s3_gpio.h"

/**
 * The interrupt of the GPIO: dmod calls this handler when the firmware gets the line ESP32S3_GPIO_CPU_INTR of the CPU. The
 * module routes the source of the GPIO to that line when a pin asks for an interrupt (set_interrupt_trigger).
 */
DMOD_IRQ_HANDLER(2)    /* ESP32S3_GPIO_CPU_INTR */
{
    esp32s3_gpio_irq_handler();
}

/**
 * @brief Initialize the dmgpio port module (ESP32-S3)
 *
 * The GPIO matrix and the IO MUX are always clocked, there is nothing to enable here.
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMGPIO port module initialized (ESP32-S3)\n");
    return 0;
}

int dmod_deinit(void)
{
    esp32s3_gpio_irq_shutdown();
    Dmod_Printf("DMGPIO port module deinitialized (ESP32-S3)\n");
    return 0;
}
