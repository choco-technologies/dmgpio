#ifndef DMGPIO_H
#define DMGPIO_H

#include <stdint.h>
#include "dmod.h"
#include "dmgpio_defs.h"

/**
 * @brief GPIO pin mode
 */
typedef enum
{
    dmgpio_mode_input = 0,      /**< Input mode */
    dmgpio_mode_output_pp,      /**< Output push-pull mode */
    dmgpio_mode_output_od,      /**< Output open-drain mode */
    dmgpio_mode_af_pp,          /**< Alternate function push-pull mode */
    dmgpio_mode_af_od,          /**< Alternate function open-drain mode */
    dmgpio_mode_analog,         /**< Analog mode */
} dmgpio_mode_t;

/**
 * @brief GPIO pin pull resistor configuration
 */
typedef enum
{
    dmgpio_pull_none = 0,       /**< No pull resistor */
    dmgpio_pull_up,             /**< Pull-up resistor */
    dmgpio_pull_down,           /**< Pull-down resistor */
} dmgpio_pull_t;

/**
 * @brief GPIO output speed
 */
typedef enum
{
    dmgpio_speed_low = 0,       /**< Low speed */
    dmgpio_speed_medium,        /**< Medium speed */
    dmgpio_speed_high,          /**< High speed */
    dmgpio_speed_very_high,     /**< Very high speed */
} dmgpio_speed_t;

/**
 * @brief GPIO pin state
 */
typedef enum
{
    dmgpio_pin_reset = 0,       /**< Pin is low */
    dmgpio_pin_set,             /**< Pin is high */
} dmgpio_pin_state_t;

/**
 * @brief IOCTL commands for DMGPIO device
 */
typedef enum
{
    dmgpio_ioctl_cmd_get_state = 1,     /**< Get current pin state */
    dmgpio_ioctl_cmd_set_state,         /**< Set pin state */
    dmgpio_ioctl_cmd_toggle,            /**< Toggle pin state */
    dmgpio_ioctl_cmd_get_mode,          /**< Get pin mode */
    dmgpio_ioctl_cmd_set_mode,          /**< Set pin mode */
    dmgpio_ioctl_cmd_get_pull,          /**< Get pull resistor configuration */
    dmgpio_ioctl_cmd_set_pull,          /**< Set pull resistor configuration */
    dmgpio_ioctl_cmd_get_speed,         /**< Get output speed */
    dmgpio_ioctl_cmd_set_speed,         /**< Set output speed */
    dmgpio_ioctl_cmd_reconfigure,       /**< Reconfigure pin with current settings */

    dmgpio_ioctl_cmd_max

} dmgpio_ioctl_cmd_t;

#endif // DMGPIO_H
