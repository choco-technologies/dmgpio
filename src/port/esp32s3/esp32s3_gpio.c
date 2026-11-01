#include "esp32s3_gpio.h"
#include "dmod.h"
#include "dmgpio_port.h"
#include <errno.h>
#include <stddef.h>

/* ---- Software state ---- */

/** Pins in use, by port. */
static dmgpio_pins_mask_t s_pins_used[ESP32S3_GPIO_PORTS] = {0};
/** Protected pins that were unlocked by the configuration (protection=unlock), by port. */
static dmgpio_pins_mask_t s_pins_unlocked[ESP32S3_GPIO_PORTS] = {0};

static const dmgpio_pins_mask_t s_valid_pins[ESP32S3_GPIO_PORTS] =
{
    ESP32S3_GPIO_VALID_PINS_A, ESP32S3_GPIO_VALID_PINS_B, ESP32S3_GPIO_VALID_PINS_C, ESP32S3_GPIO_VALID_PINS_D
};

static const dmgpio_pins_mask_t s_protected_pins[ESP32S3_GPIO_PORTS] =
{
    0U, ESP32S3_GPIO_PROTECTED_PINS_B, ESP32S3_GPIO_PROTECTED_PINS_C, 0U
};

/* The GPIO matrix keeps GPIO0-31 in the first bank of registers and GPIO32-48 in the second one; a port is half of a bank */
typedef struct
{
    uint32_t out;
    uint32_t out_w1ts;
    uint32_t out_w1tc;
    uint32_t enable;
    uint32_t enable_w1ts;
    uint32_t enable_w1tc;
    uint32_t in;
} bank_regs_t;

static const bank_regs_t s_banks[2] =
{
    { ESP32S3_GPIO_OUT_REG,  ESP32S3_GPIO_OUT_W1TS_REG,  ESP32S3_GPIO_OUT_W1TC_REG,
      ESP32S3_GPIO_ENABLE_REG,  ESP32S3_GPIO_ENABLE_W1TS_REG,  ESP32S3_GPIO_ENABLE_W1TC_REG,  ESP32S3_GPIO_IN_REG },
    { ESP32S3_GPIO_OUT1_REG, ESP32S3_GPIO_OUT1_W1TS_REG, ESP32S3_GPIO_OUT1_W1TC_REG,
      ESP32S3_GPIO_ENABLE1_REG, ESP32S3_GPIO_ENABLE1_W1TS_REG, ESP32S3_GPIO_ENABLE1_W1TC_REG, ESP32S3_GPIO_IN1_REG }
};

/* ---- Internal helpers ---- */

static inline volatile uint32_t *reg(uint32_t address)
{
    return (volatile uint32_t *)address;
}

static inline const bank_regs_t *bank_of(dmgpio_port_t port)
{
    return &s_banks[port >> 1];
}

/** Position of the pins of a port in the register of its bank. */
static inline uint32_t shift_of(dmgpio_port_t port)
{
    return (port & 1U) ? 16U : 0U;
}

/** Number of the pad (GPIO) of a pin. */
static inline uint32_t pad_of(dmgpio_port_t port, int pin)
{
    return (uint32_t)port * 16U + (uint32_t)pin;
}

static int is_valid_port(dmgpio_port_t port)
{
    return (uint32_t)port < ESP32S3_GPIO_PORTS;
}

/** A port that exists and pins that all exist (a non-empty mask for the commands that need a pin). */
static int check_pins(dmgpio_port_t port, dmgpio_pins_mask_t pins)
{
    if (!is_valid_port(port)) return -1;
    if ((pins & (dmgpio_pins_mask_t)~s_valid_pins[port]) != 0U) return -1;
    return 0;
}

/** Changes the fields of the IO MUX register of each selected pad: reg = (reg & ~clear) | set. */
static void iomux_update(dmgpio_port_t port, dmgpio_pins_mask_t pins, uint32_t clear, uint32_t set)
{
    for (int pin = 0; pin < 16; pin++)
    {
        if (!(pins & (dmgpio_pins_mask_t)(1U << pin))) continue;
        volatile uint32_t *r = reg(ESP32S3_IO_MUX_REG(pad_of(port, pin)));
        *r = (*r & ~clear) | set;
    }
}

/** The IO MUX register of the lowest selected pad, or 0 when there is none. */
static uint32_t iomux_read(dmgpio_port_t port, dmgpio_pins_mask_t pins, int *found)
{
    for (int pin = 0; pin < 16; pin++)
    {
        if (pins & (dmgpio_pins_mask_t)(1U << pin))
        {
            *found = 1;
            return *reg(ESP32S3_IO_MUX_REG(pad_of(port, pin)));
        }
    }
    *found = 0;
    return 0U;
}

/* ======================================================================
 *  Interrupts - not supported yet
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _add_interrupt_handler,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_port_interrupt_handler_t handler, void *user_ptr ))
{
    (void)port; (void)pins; (void)handler; (void)user_ptr;
    return -ENOTSUP;
}

dmod_dmgpio_port_api_declaration(1.0, int, _remove_interrupt_handler,
    ( dmgpio_port_t port, void *user_ptr ))
{
    (void)user_ptr;
    return is_valid_port(port) ? 0 : -1;    /* there is nothing registered */
}

/* ======================================================================
 *  Configuration session
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _begin_configuration,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return check_pins(port, pins);
}

dmod_dmgpio_port_api_declaration(1.0, int, _finish_configuration,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    return check_pins(port, pins);
}

/* ======================================================================
 *  Clock / power
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _set_power,
    ( dmgpio_port_t port, int power_on ))
{
    (void)power_on;
    /* The GPIO matrix and the IO MUX have no clock gate */
    return is_valid_port(port) ? 0 : -1;
}

/* ======================================================================
 *  Pin protection
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, bool, _are_pins_protected,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return false;
    return (pins & s_protected_pins[port] & (dmgpio_pins_mask_t)~s_pins_unlocked[port]) != 0U;
}

dmod_dmgpio_port_api_declaration(1.0, int, _unlock_protection,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_protection_t protection ))
{
    if (!is_valid_port(port)) return -1;
    if (protection == dmgpio_protection_unlock_protected_pins)
        s_pins_unlocked[port] |= pins & s_protected_pins[port];
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _lock_protection,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return -1;
    s_pins_unlocked[port] &= (dmgpio_pins_mask_t)~pins;
    return 0;
}

/* ======================================================================
 *  Configuration parameters (require begin/finish_configuration)
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _set_speed,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t speed ))
{
    if (check_pins(port, pins) != 0) return -1;
    /* The pads of the ESP32-S3 have no slew rate setting, the strength of the output is dmgpio's "current" */
    return speed == dmgpio_speed_default ? 0 : -ENOTSUP;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_speed,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t *out_speed ))
{
    if (check_pins(port, pins) != 0 || out_speed == NULL) return -1;
    *out_speed = dmgpio_speed_default;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_current,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t current ))
{
    if (check_pins(port, pins) != 0) return -1;
    uint32_t drv;
    switch (current)
    {
        case dmgpio_current_default: return 0;
        case dmgpio_current_minimum: drv = 0U; break;   /* ~5 mA */
        case dmgpio_current_medium:  drv = 2U; break;   /* ~20 mA, the default of the pad */
        case dmgpio_current_maximum: drv = 3U; break;   /* ~40 mA */
        default:                     return -1;
    }
    iomux_update(port, pins, ESP32S3_IO_MUX_FUN_DRV_M, drv << ESP32S3_IO_MUX_FUN_DRV_S);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_current,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t *out_current ))
{
    if (check_pins(port, pins) != 0 || out_current == NULL || pins == 0U) return -1;
    int found;
    uint32_t drv = (iomux_read(port, pins, &found) & ESP32S3_IO_MUX_FUN_DRV_M) >> ESP32S3_IO_MUX_FUN_DRV_S;
    *out_current = drv == 0U ? dmgpio_current_minimum : drv == 3U ? dmgpio_current_maximum : dmgpio_current_medium;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_mode,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t mode ))
{
    if (check_pins(port, pins) != 0) return -1;
    const bank_regs_t *bank = bank_of(port);
    uint32_t mask = (uint32_t)pins << shift_of(port);

    switch (mode)
    {
        case dmgpio_mode_default:
            return 0;
        case dmgpio_mode_input:
            iomux_update(port, pins, ESP32S3_IO_MUX_MCU_SEL_M,
                         (ESP32S3_IO_MUX_FUNC_GPIO << ESP32S3_IO_MUX_MCU_SEL_S) | ESP32S3_IO_MUX_FUN_IE);
            *reg(bank->enable_w1tc) = mask;
            return 0;
        case dmgpio_mode_output:
            for (int pin = 0; pin < 16; pin++)
            {
                if (pins & (dmgpio_pins_mask_t)(1U << pin))
                    *reg(ESP32S3_GPIO_FUNC_OUT_SEL_REG(pad_of(port, pin))) = ESP32S3_GPIO_SIG_GPIO_OUT;
            }
            /* The input stays enabled: the state of the pad can be read back */
            iomux_update(port, pins, ESP32S3_IO_MUX_MCU_SEL_M,
                         (ESP32S3_IO_MUX_FUNC_GPIO << ESP32S3_IO_MUX_MCU_SEL_S) | ESP32S3_IO_MUX_FUN_IE);
            *reg(bank->enable_w1ts) = mask;
            return 0;
        case dmgpio_mode_alternate:
            /* The pad is driven by the function chosen in the IO MUX (set_alternate_function), not by the GPIO matrix */
            *reg(bank->enable_w1tc) = mask;
            iomux_update(port, pins, 0U, ESP32S3_IO_MUX_FUN_IE);
            return 0;
        default:
            return -1;
    }
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_mode,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t *out_mode ))
{
    if (check_pins(port, pins) != 0 || out_mode == NULL || pins == 0U) return -1;
    int found;
    uint32_t iomux = iomux_read(port, pins, &found);
    if (((iomux & ESP32S3_IO_MUX_MCU_SEL_M) >> ESP32S3_IO_MUX_MCU_SEL_S) != ESP32S3_IO_MUX_FUNC_GPIO)
    {
        *out_mode = dmgpio_mode_alternate;
        return 0;
    }
    uint32_t lowest = (uint32_t)pins & (0U - (uint32_t)pins);
    uint32_t enabled = *reg(bank_of(port)->enable) >> shift_of(port);
    *out_mode = (enabled & lowest) ? dmgpio_mode_output : dmgpio_mode_input;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_pull,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t pull ))
{
    if (check_pins(port, pins) != 0) return -1;
    switch (pull)
    {
        case dmgpio_pull_default:
            return 0;
        case dmgpio_pull_up:
            iomux_update(port, pins, ESP32S3_IO_MUX_FUN_PD, ESP32S3_IO_MUX_FUN_PU);
            return 0;
        case dmgpio_pull_down:
            iomux_update(port, pins, ESP32S3_IO_MUX_FUN_PU, ESP32S3_IO_MUX_FUN_PD);
            return 0;
        default:
            return -1;
    }
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_pull,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t *out_pull ))
{
    if (check_pins(port, pins) != 0 || out_pull == NULL || pins == 0U) return -1;
    int found;
    uint32_t iomux = iomux_read(port, pins, &found);
    *out_pull = (iomux & ESP32S3_IO_MUX_FUN_PU) ? dmgpio_pull_up : (iomux & ESP32S3_IO_MUX_FUN_PD) ? dmgpio_pull_down : dmgpio_pull_default;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_output_circuit,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t oc ))
{
    if (check_pins(port, pins) != 0) return -1;
    if (oc == dmgpio_output_circuit_default) return 0;
    if (oc != dmgpio_output_circuit_open_drain && oc != dmgpio_output_circuit_push_pull) return -1;
    for (int pin = 0; pin < 16; pin++)
    {
        if (!(pins & (dmgpio_pins_mask_t)(1U << pin))) continue;
        volatile uint32_t *r = reg(ESP32S3_GPIO_PIN_REG(pad_of(port, pin)));
        if (oc == dmgpio_output_circuit_open_drain)
            *r |= ESP32S3_GPIO_PIN_PAD_DRIVER;
        else
            *r &= ~ESP32S3_GPIO_PIN_PAD_DRIVER;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_output_circuit,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t *out_oc ))
{
    if (check_pins(port, pins) != 0 || out_oc == NULL || pins == 0U) return -1;
    for (int pin = 0; pin < 16; pin++)
    {
        if (pins & (dmgpio_pins_mask_t)(1U << pin))
        {
            uint32_t v = *reg(ESP32S3_GPIO_PIN_REG(pad_of(port, pin)));
            *out_oc = (v & ESP32S3_GPIO_PIN_PAD_DRIVER) ? dmgpio_output_circuit_open_drain : dmgpio_output_circuit_push_pull;
            return 0;
        }
    }
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_alternate_function,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, uint8_t af ))
{
    /* The function of the IO MUX: 0-5 (1 is the GPIO matrix); the peripherals that are only in the GPIO matrix are routed by their drivers */
    if (check_pins(port, pins) != 0 || af > ESP32S3_IO_MUX_FUNC_MAX) return -1;
    iomux_update(port, pins, ESP32S3_IO_MUX_MCU_SEL_M, ((uint32_t)af << ESP32S3_IO_MUX_MCU_SEL_S) | ESP32S3_IO_MUX_FUN_IE);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_alternate_function,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, uint8_t *out_af ))
{
    if (check_pins(port, pins) != 0 || out_af == NULL || pins == 0U) return -1;
    int found;
    *out_af = (uint8_t)((iomux_read(port, pins, &found) & ESP32S3_IO_MUX_MCU_SEL_M) >> ESP32S3_IO_MUX_MCU_SEL_S);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_interrupt_trigger,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t trigger ))
{
    if (check_pins(port, pins) != 0) return -1;
    return trigger == dmgpio_int_trigger_off ? 0 : -ENOTSUP;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_interrupt_trigger,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t *out_trigger ))
{
    if (check_pins(port, pins) != 0 || out_trigger == NULL) return -1;
    *out_trigger = dmgpio_int_trigger_off;
    return 0;
}

/* ======================================================================
 *  Pin usage tracking
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _set_pins_used,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return -1;
    s_pins_used[port] |= pins;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_pins_unused,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return -1;
    s_pins_used[port] &= (dmgpio_pins_mask_t)~pins;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _check_is_pin_used,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, int *out_used ))
{
    if (!is_valid_port(port) || out_used == NULL) return -1;
    *out_used = (s_pins_used[port] & pins) != 0U;
    return 0;
}

/* ======================================================================
 *  Data read / write
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _write_data,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t data ))
{
    if (!is_valid_port(port)) return -1;
    const bank_regs_t *bank = bank_of(port);
    uint32_t sh = shift_of(port);
    pins &= s_valid_pins[port];
    *reg(bank->out_w1ts) = (uint32_t)(data & pins) << sh;
    *reg(bank->out_w1tc) = (uint32_t)((dmgpio_pins_mask_t)~data & pins) << sh;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_data,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t *out_data ))
{
    if (!is_valid_port(port) || out_data == NULL) return -1;
    *out_data = (dmgpio_pins_mask_t)((*reg(bank_of(port)->in) >> shift_of(port)) & pins & s_valid_pins[port]);
    return 0;
}

/* ======================================================================
 *  Pin state operations (no argument checking - the caller must validate)
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_high_state_pins,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return 0U;
    return (dmgpio_pins_mask_t)((*reg(bank_of(port)->in) >> shift_of(port)) & pins & s_valid_pins[port]);
}

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_low_state_pins,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return 0U;
    return (dmgpio_pins_mask_t)(~(*reg(bank_of(port)->in) >> shift_of(port)) & pins & s_valid_pins[port]);
}

dmod_dmgpio_port_api_declaration(1.0, void, _set_pins_state,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_state_t state ))
{
    if (!is_valid_port(port)) return;
    uint32_t mask = (uint32_t)(pins & s_valid_pins[port]) << shift_of(port);
    *reg(state == dmgpio_pins_state_all_high ? bank_of(port)->out_w1ts : bank_of(port)->out_w1tc) = mask;
}

dmod_dmgpio_port_api_declaration(1.0, void, _toggle_pins_state,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return;
    const bank_regs_t *bank = bank_of(port);
    uint32_t sh = shift_of(port);
    uint32_t mask = (uint32_t)(pins & s_valid_pins[port]) << sh;
    uint32_t high = *reg(bank->out) & mask;
    *reg(bank->out_w1ts) = mask & ~high;
    *reg(bank->out_w1tc) = high;
}
