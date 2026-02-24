#ifndef DMGPIO_PORT_H
#define DMGPIO_PORT_H

#include "dmod.h"
#include "dmgpio_port_defs.h"

/**
 * @brief GPIO port identifier type (0=GPIOA, 1=GPIOB, ...)
 */
typedef uint8_t dmgpio_port_t;

/**
 * @brief GPIO pin number type (0-15)
 */
typedef uint8_t dmgpio_pin_t;

dmod_dmgpio_port_api(1.0, int, _init, ( dmgpio_port_t port, dmgpio_pin_t pin, dmgpio_mode_t mode, dmgpio_pull_t pull, dmgpio_speed_t speed, uint8_t alternate ));
dmod_dmgpio_port_api(1.0, int, _deinit, ( dmgpio_port_t port, dmgpio_pin_t pin ));
dmod_dmgpio_port_api(1.0, dmgpio_pin_state_t, _read_pin, ( dmgpio_port_t port, dmgpio_pin_t pin ));
dmod_dmgpio_port_api(1.0, void, _write_pin, ( dmgpio_port_t port, dmgpio_pin_t pin, dmgpio_pin_state_t state ));
dmod_dmgpio_port_api(1.0, void, _toggle_pin, ( dmgpio_port_t port, dmgpio_pin_t pin ));

#endif // DMGPIO_PORT_H
