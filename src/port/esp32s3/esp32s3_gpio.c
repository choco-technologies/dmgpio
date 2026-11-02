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
 *  Interrupts
 * ====================================================================== */

/** Maximum number of interrupt handlers per port (each dmgpio context that uses interrupts on a port takes one). */
#define ESP32S3_PORT_MAX_IRQ_HANDLERS   8U

typedef struct
{
    dmgpio_port_interrupt_handler_t handler;
    void                           *user_ptr;
    dmgpio_pins_mask_t              pins;
} irq_entry_t;

static irq_entry_t s_handlers[ESP32S3_GPIO_PORTS][ESP32S3_PORT_MAX_IRQ_HANDLERS];
/** Pins with the interrupt turned on, by port. */
static dmgpio_pins_mask_t s_irq_pins[ESP32S3_GPIO_PORTS] = {0};

static inline uint32_t intenable_read(void)
{
    uint32_t value;
    __asm__ __volatile__("rsr.intenable %0" : "=a"(value));
    return value;
}

static inline void intenable_write(uint32_t value)
{
    __asm__ __volatile__("wsr.intenable %0; rsync" :: "a"(value) : "memory");
}

/** Routes the interrupt of the GPIO to the CPU and turns it on when a pin needs it, turns it off when none does. */
static void irq_line_update(void)
{
    int needed = 0;
    for (uint32_t p = 0; p < ESP32S3_GPIO_PORTS; p++)
        if (s_irq_pins[p] != 0U) needed = 1;

    Dmod_EnterCritical();
    if (needed)
    {
        *reg(ESP32S3_INTMTX_GPIO_MAP_REG) = ESP32S3_GPIO_CPU_INTR;
        intenable_write(intenable_read() | (1UL << ESP32S3_GPIO_CPU_INTR));
    }
    else
    {
        intenable_write(intenable_read() & ~(1UL << ESP32S3_GPIO_CPU_INTR));
    }
    Dmod_ExitCritical();
}

/** The interrupt status register (and its write-1-to-clear one) of the bank of a port. */
static inline void irq_status_clear(dmgpio_port_t port, dmgpio_pins_mask_t pins)
{
    *reg((port >> 1) ? ESP32S3_GPIO_STATUS1_W1TC_REG : ESP32S3_GPIO_STATUS_W1TC_REG) = (uint32_t)pins << shift_of(port);
}

static inline dmgpio_pins_mask_t irq_status_read(dmgpio_port_t port)
{
    return (dmgpio_pins_mask_t)(*reg((port >> 1) ? ESP32S3_GPIO_STATUS1_REG : ESP32S3_GPIO_STATUS_REG) >> shift_of(port));
}

void esp32s3_gpio_irq_handler(void)
{
    for (dmgpio_port_t port = 0; port < ESP32S3_GPIO_PORTS; port++)
    {
        /* Only the pins that this module turned on: the status of the others is not ours to clear */
        dmgpio_pins_mask_t pending = irq_status_read(port) & s_irq_pins[port];
        if (pending == 0U) continue;

        irq_status_clear(port, pending);        /* before the handlers: an edge that comes during them is kept */
        dmgpio_pins_mask_t state = (dmgpio_pins_mask_t)((*reg(bank_of(port)->in) >> shift_of(port)) & pending);
        for (uint32_t i = 0; i < ESP32S3_PORT_MAX_IRQ_HANDLERS; i++)
        {
            dmgpio_pins_mask_t match = s_handlers[port][i].pins & pending;
            if (s_handlers[port][i].handler != NULL && match != 0U)
                s_handlers[port][i].handler(s_handlers[port][i].user_ptr, port, match, state);
        }
    }
}

void esp32s3_gpio_irq_shutdown(void)
{
    for (dmgpio_port_t port = 0; port < ESP32S3_GPIO_PORTS; port++)
    {
        for (int pin = 0; pin < 16; pin++)
        {
            if (!(s_irq_pins[port] & (dmgpio_pins_mask_t)(1U << pin))) continue;
            volatile uint32_t *r = reg(ESP32S3_GPIO_PIN_REG(pad_of(port, pin)));
            *r &= ~(ESP32S3_GPIO_PIN_INT_TYPE_M | ESP32S3_GPIO_PIN_INT_ENA_CPU);
        }
        irq_status_clear(port, s_irq_pins[port]);
        s_irq_pins[port] = 0U;
        for (uint32_t i = 0; i < ESP32S3_PORT_MAX_IRQ_HANDLERS; i++)
            s_handlers[port][i].handler = NULL;
    }
    irq_line_update();
}

dmod_dmgpio_port_api_declaration(1.0, int, _add_interrupt_handler,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_port_interrupt_handler_t handler, void *user_ptr ))
{
    if (!is_valid_port(port) || handler == NULL) return -1;
    for (uint32_t i = 0; i < ESP32S3_PORT_MAX_IRQ_HANDLERS; i++)
    {
        if (s_handlers[port][i].handler == NULL)
        {
            s_handlers[port][i].user_ptr = user_ptr;
            s_handlers[port][i].pins     = pins;
            s_handlers[port][i].handler  = handler;     /* last: the interrupt sees a complete entry */
            return 0;
        }
    }
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int, _remove_interrupt_handler,
    ( dmgpio_port_t port, void *user_ptr ))
{
    if (!is_valid_port(port)) return -1;
    for (uint32_t i = 0; i < ESP32S3_PORT_MAX_IRQ_HANDLERS; i++)
    {
        if (s_handlers[port][i].handler != NULL && s_handlers[port][i].user_ptr == user_ptr)
            s_handlers[port][i].handler = NULL;
    }
    return 0;
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
    uint32_t type;
    switch (trigger)
    {
        case dmgpio_int_trigger_off:          type = 0U; break;
        case dmgpio_int_trigger_rising_edge:  type = 1U; break;
        case dmgpio_int_trigger_falling_edge: type = 2U; break;
        case dmgpio_int_trigger_both_edges:   type = 3U; break;
        /* The levels are refused like on the STM32: the status stays set while the level does, and the handler of a pin cannot
         * take the level away - the interrupt would come again at once and starve everything else. */
        default:                              return -1;
    }

    for (int pin = 0; pin < 16; pin++)
    {
        if (!(pins & (dmgpio_pins_mask_t)(1U << pin))) continue;
        volatile uint32_t *r = reg(ESP32S3_GPIO_PIN_REG(pad_of(port, pin)));
        uint32_t v = *r & ~(ESP32S3_GPIO_PIN_INT_TYPE_M | ESP32S3_GPIO_PIN_INT_ENA_CPU);
        if (type == 0U)
        {
            *r = v;
        }
        else
        {
            *r = v | (type << ESP32S3_GPIO_PIN_INT_TYPE_S);
        }
    }

    dmgpio_pins_mask_t one = (dmgpio_pins_mask_t)(pins & s_valid_pins[port]);
    irq_status_clear(port, one);                /* an edge from before is not an interrupt of the new setting */
    if (type == 0U)
    {
        s_irq_pins[port] &= (dmgpio_pins_mask_t)~one;
    }
    else
    {
        s_irq_pins[port] |= one;
        for (int pin = 0; pin < 16; pin++)
        {
            if (one & (dmgpio_pins_mask_t)(1U << pin))
                *reg(ESP32S3_GPIO_PIN_REG(pad_of(port, pin))) |= ESP32S3_GPIO_PIN_INT_ENA_CPU;
        }
    }
    irq_line_update();
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_interrupt_trigger,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t *out_trigger ))
{
    if (check_pins(port, pins) != 0 || out_trigger == NULL || pins == 0U) return -1;
    /* Read from the lowest selected pin */
    for (int pin = 0; pin < 16; pin++)
    {
        if (!(pins & (dmgpio_pins_mask_t)(1U << pin))) continue;
        uint32_t v = *reg(ESP32S3_GPIO_PIN_REG(pad_of(port, pin)));
        uint32_t type = (v & ESP32S3_GPIO_PIN_INT_TYPE_M) >> ESP32S3_GPIO_PIN_INT_TYPE_S;
        if (!(v & ESP32S3_GPIO_PIN_INT_ENA_CPU)) type = 0U;
        *out_trigger = type == 1U ? dmgpio_int_trigger_rising_edge
                     : type == 2U ? dmgpio_int_trigger_falling_edge
                     : type == 3U ? dmgpio_int_trigger_both_edges
                     : dmgpio_int_trigger_off;
        return 0;
    }
    return -1;
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
