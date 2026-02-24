#include "stm32_common.h"
#include <stddef.h>

dmod_dmgpio_port_api_declaration(1.0, int,  _set_driver_interrupt_handler, ( dmgpio_interrupt_handler_t handler ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _begin_configuration,  ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _finish_configuration, ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _set_power, ( dmgpio_port_t port, int power_on ))
{
    return -1;
}

/* --- Pin protection --- */

dmod_dmgpio_port_api_declaration(1.0, bool, _are_pins_protected, ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return false;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _unlock_protection,  ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_protection_t protection ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _lock_protection,    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _set_speed,             ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t speed ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _read_speed,            ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t *out_speed ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _set_current,           ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t current ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _read_current,          ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t *out_current ))
{
    return -1;
}
dmod_dmgpio_port_api_declaration(1.0, int,  _set_mode,              ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t mode ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _read_mode,             ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t *out_mode ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _set_pull,              ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t pull ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _read_pull,             ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t *out_pull ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _set_output_circuit,    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t oc ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _read_output_circuit,   ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t *out_oc ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _set_interrupt_trigger, ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t trigger ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _read_interrupt_trigger,( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t *out_trigger ))
{
    return -1;
}

/* --- Pin usage tracking --- */

dmod_dmgpio_port_api_declaration(1.0, int,  _set_pins_used,    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _set_pins_unused,  ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _check_is_pin_used,( dmgpio_port_t port, dmgpio_pins_mask_t pins, int *out_used ))
{
    return -1;
}

/* --- Data read/write --- */
dmod_dmgpio_port_api_declaration(1.0, int,  _write_data,          ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t data ))
{
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int,  _read_data,           ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t *out_data ))
{
    return -1;
}

/* --- Pin state operations (no argument checking, must ensure correct pins) --- */

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_high_state_pins, ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_low_state_pins,  ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, void, _set_pins_state,      ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_state_t state ))
{
    // Do nothing
}

dmod_dmgpio_port_api_declaration(1.0, void, _toggle_pins_state,   ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    // Do nothing
}
