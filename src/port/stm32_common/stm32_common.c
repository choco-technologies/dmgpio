#include "stm32_common.h"
#include <stddef.h>

/* ---- Software state ---- */

/** Bitmask of pins currently in use, indexed by port number. */
static dmgpio_pins_mask_t s_pins_used[STM32_MAX_PORTS] = {0};

/** Registered GPIO port interrupt handler (NULL if not set). */
static dmgpio_port_interrupt_handler_t s_interrupt_handler = NULL;

/* ---- Internal helpers ---- */

static int is_valid_port(dmgpio_port_t port)
{
    return ((uint32_t)port < STM32_MAX_PORTS);
}

/**
 * @brief Write a 2-bit value into each selected pin field of a register.
 *
 * Each pin occupies two consecutive bits starting at bit (pin * 2).
 *
 * @param reg   Target register pointer.
 * @param pins  Bitmask selecting which pins to update.
 * @param value 2-bit value (0–3) to write for every selected pin.
 */
static void set_2bit_fields(volatile uint32_t *reg, dmgpio_pins_mask_t pins, uint32_t value)
{
    uint32_t val = *reg;
    for (int pin = 0; pin < 16; pin++)
    {
        if (pins & (dmgpio_pins_mask_t)(1U << pin))
        {
            uint32_t shift = (uint32_t)pin * 2U;
            val &= ~(3U << shift);
            val |= (value & 3U) << shift;
        }
    }
    *reg = val;
}

/**
 * @brief Read the 2-bit field of the lowest set pin in a register.
 *
 * @param reg  Source register pointer.
 * @param pins Bitmask; the lowest set bit selects the pin to read.
 * @return The 2-bit field value (0–3), or 0 if @p pins is empty.
 */
static uint32_t read_2bit_field(volatile const uint32_t *reg, dmgpio_pins_mask_t pins)
{
    for (int pin = 0; pin < 16; pin++)
    {
        if (pins & (dmgpio_pins_mask_t)(1U << pin))
            return (*reg >> ((uint32_t)pin * 2U)) & 3U;
    }
    return 0U;
}

/* ---- NVIC helpers ---- */

static void nvic_enable_irq(uint32_t irqn)
{
    STM32_NVIC_ISER[irqn >> 5U] = 1U << (irqn & 0x1FU);
}

static void nvic_disable_irq(uint32_t irqn)
{
    STM32_NVIC_ICER[irqn >> 5U] = 1U << (irqn & 0x1FU);
}

/**
 * @brief Return the NVIC IRQ number for a given EXTI pin line.
 *
 * EXTI0–4 have individual IRQs; EXTI5–9 share EXTI9_5_IRQn (23);
 * EXTI10–15 share EXTI15_10_IRQn (40). IRQ numbers are identical for
 * STM32F4 and STM32F7.
 */
static uint32_t exti_pin_to_irqn(int pin)
{
    if (pin <= 4) return (uint32_t)(6 + pin);   /* EXTI0_IRQn=6 .. EXTI4_IRQn=10 */
    if (pin <= 9) return 23U;                   /* EXTI9_5_IRQn */
    return 40U;                                 /* EXTI15_10_IRQn */
}

/* ======================================================================
 *  Driver interrupt handler
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _set_driver_interrupt_handler,
    ( dmgpio_port_interrupt_handler_t handler ))
{
    s_interrupt_handler = handler;
    return 0;
}

/* ======================================================================
 *  Configuration session
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _begin_configuration,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return -1;
    (void)pins;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _finish_configuration,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return -1;
    (void)pins;
    return 0;
}

/* ======================================================================
 *  Clock / power
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _set_power,
    ( dmgpio_port_t port, int power_on ))
{
    if (!is_valid_port(port)) return -1;
    /*
     * Note: read-modify-write on RCC_AHB1ENR is not atomic.
     * This function should be called only from a single context during
     * initialisation/deinitialisation to avoid race conditions.
     */
    if (power_on)
        STM32_RCC_AHB1ENR |= (1U << (uint32_t)port);
    else
        STM32_RCC_AHB1ENR &= ~(1U << (uint32_t)port);
    return 0;
}

/* ======================================================================
 *  Pin protection
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, bool, _are_pins_protected,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return false;
    uint32_t lckr = STM32_GPIO(port)->LCKR;
    /* LCKK (bit 16) indicates that the lock sequence has been applied. */
    if (!(lckr & (1UL << 16U))) return false;
    return (lckr & (uint32_t)pins) != 0U;
}

dmod_dmgpio_port_api_declaration(1.0, int, _unlock_protection,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_protection_t protection ))
{
    /*
     * On STM32, hardware locking via LCKR cannot be reversed without a
     * reset.  This function allows the higher layer to bypass the
     * protection check; the actual hardware action is a no-op here.
     */
    if (!is_valid_port(port)) return -1;
    (void)pins;
    (void)protection;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _lock_protection,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return -1;
    volatile stm32_gpio_t *gpio = STM32_GPIO(port);
    uint32_t lckval = (1UL << 16U) | (uint32_t)pins;
    /* STM32 LCKR write sequence: LCKK=1, LCKK=0, LCKK=1, then read. */
    gpio->LCKR = lckval;
    gpio->LCKR = (uint32_t)pins;
    gpio->LCKR = lckval;
    (void)gpio->LCKR;
    /* LCKK must be set after a successful lock. */
    if (!(gpio->LCKR & (1UL << 16U))) return -1;
    return 0;
}

/* ======================================================================
 *  Configuration parameters (require begin/finish_configuration)
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, int, _set_speed,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t speed ))
{
    if (!is_valid_port(port)) return -1;
    uint32_t ospeedr_val;
    switch (speed)
    {
        case dmgpio_speed_default:  return 0;          /* leave hardware default */
        case dmgpio_speed_minimum:  ospeedr_val = 0U; break;
        case dmgpio_speed_medium:   ospeedr_val = 1U; break;
        case dmgpio_speed_maximum:  ospeedr_val = 3U; break;
        default:                    return -1;
    }
    set_2bit_fields(&STM32_GPIO(port)->OSPEEDR, pins, ospeedr_val);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_speed,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t *out_speed ))
{
    if (!is_valid_port(port) || out_speed == NULL || pins == 0U) return -1;
    switch (read_2bit_field(&STM32_GPIO(port)->OSPEEDR, pins))
    {
        case 0U: *out_speed = dmgpio_speed_minimum; break;
        case 1U: *out_speed = dmgpio_speed_medium;  break;
        default: *out_speed = dmgpio_speed_maximum; break;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_current,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t current ))
{
    /*
     * STM32F4/F7 does not expose a dedicated output-current register;
     * drive strength is solely determined by the output speed (OSPEEDR).
     * Accept the call and succeed without touching hardware.
     */
    if (!is_valid_port(port)) return -1;
    (void)pins;
    (void)current;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_current,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t *out_current ))
{
    if (!is_valid_port(port) || out_current == NULL) return -1;
    (void)pins;
    *out_current = dmgpio_current_default;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_mode,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t mode ))
{
    if (!is_valid_port(port)) return -1;
    uint32_t moder_val;
    switch (mode)
    {
        case dmgpio_mode_default:   return 0;          /* leave hardware default */
        case dmgpio_mode_input:     moder_val = 0U; break;
        case dmgpio_mode_output:    moder_val = 1U; break;
        case dmgpio_mode_alternate: moder_val = 2U; break;
        default:                    return -1;
    }
    set_2bit_fields(&STM32_GPIO(port)->MODER, pins, moder_val);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_mode,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t *out_mode ))
{
    if (!is_valid_port(port) || out_mode == NULL || pins == 0U) return -1;
    switch (read_2bit_field(&STM32_GPIO(port)->MODER, pins))
    {
        case 0U: *out_mode = dmgpio_mode_input;     break;
        case 1U: *out_mode = dmgpio_mode_output;    break;
        case 2U: *out_mode = dmgpio_mode_alternate; break;
        default: *out_mode = dmgpio_mode_default;   break;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_pull,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t pull ))
{
    if (!is_valid_port(port)) return -1;
    uint32_t pupdr_val;
    switch (pull)
    {
        case dmgpio_pull_default: return 0;          /* leave hardware default */
        case dmgpio_pull_up:      pupdr_val = 1U; break;
        case dmgpio_pull_down:    pupdr_val = 2U; break;
        default:                  return -1;
    }
    set_2bit_fields(&STM32_GPIO(port)->PUPDR, pins, pupdr_val);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_pull,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t *out_pull ))
{
    if (!is_valid_port(port) || out_pull == NULL || pins == 0U) return -1;
    switch (read_2bit_field(&STM32_GPIO(port)->PUPDR, pins))
    {
        case 1U: *out_pull = dmgpio_pull_up;      break;
        case 2U: *out_pull = dmgpio_pull_down;    break;
        default: *out_pull = dmgpio_pull_default; break;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_output_circuit,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t oc ))
{
    if (!is_valid_port(port)) return -1;
    volatile stm32_gpio_t *gpio = STM32_GPIO(port);
    switch (oc)
    {
        case dmgpio_output_circuit_default:    return 0; /* leave hardware default */
        case dmgpio_output_circuit_push_pull:  gpio->OTYPER &= ~(uint32_t)pins; break;
        case dmgpio_output_circuit_open_drain: gpio->OTYPER |=  (uint32_t)pins; break;
        default:                               return -1;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_output_circuit,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t *out_oc ))
{
    if (!is_valid_port(port) || out_oc == NULL || pins == 0U) return -1;
    /* Read from the lowest set pin. */
    for (int pin = 0; pin < 16; pin++)
    {
        if (pins & (dmgpio_pins_mask_t)(1U << pin))
        {
            *out_oc = (STM32_GPIO(port)->OTYPER & (1U << (uint32_t)pin))
                ? dmgpio_output_circuit_open_drain
                : dmgpio_output_circuit_push_pull;
            return 0;
        }
    }
    return -1;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_interrupt_trigger,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t trigger ))
{
    if (!is_valid_port(port)) return -1;
    /* STM32 EXTI only supports edge-sensitive triggers. */
    if (trigger & (dmgpio_int_trigger_high_level | dmgpio_int_trigger_low_level))
        return -1;

    volatile stm32_exti_t *exti = STM32_EXTI;

    for (int pin = 0; pin < 16; pin++)
    {
        if (!(pins & (dmgpio_pins_mask_t)(1U << pin))) continue;

        uint32_t pin_mask = 1U << (uint32_t)pin;

        if (trigger == dmgpio_int_trigger_off)
        {
            exti->IMR  &= ~pin_mask;
            exti->RTSR &= ~pin_mask;
            exti->FTSR &= ~pin_mask;
            nvic_disable_irq(exti_pin_to_irqn(pin));
        }
        else
        {
            /* Map GPIO port to EXTI line via SYSCFG_EXTICR. */
            uint32_t exticr_idx   = (uint32_t)pin / 4U;
            uint32_t exticr_shift = ((uint32_t)pin % 4U) * 4U;
            STM32_SYSCFG_EXTICR[exticr_idx] =
                (STM32_SYSCFG_EXTICR[exticr_idx] & ~(0xFU << exticr_shift)) |
                ((uint32_t)port << exticr_shift);

            if (trigger & dmgpio_int_trigger_rising_edge)
                exti->RTSR |= pin_mask;
            else
                exti->RTSR &= ~pin_mask;

            if (trigger & dmgpio_int_trigger_falling_edge)
                exti->FTSR |= pin_mask;
            else
                exti->FTSR &= ~pin_mask;

            exti->IMR |= pin_mask;
            nvic_enable_irq(exti_pin_to_irqn(pin));
        }
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_interrupt_trigger,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t *out_trigger ))
{
    if (!is_valid_port(port) || out_trigger == NULL || pins == 0U) return -1;
    volatile stm32_exti_t *exti = STM32_EXTI;

    /* Read from the lowest set pin. */
    for (int pin = 0; pin < 16; pin++)
    {
        if (!(pins & (dmgpio_pins_mask_t)(1U << pin))) continue;

        uint32_t pin_mask = 1U << (uint32_t)pin;
        if (!(exti->IMR & pin_mask))
        {
            *out_trigger = dmgpio_int_trigger_off;
        }
        else
        {
            int rising  = (exti->RTSR & pin_mask) != 0U;
            int falling = (exti->FTSR & pin_mask) != 0U;
            if (rising && falling)
                *out_trigger = dmgpio_int_trigger_both_edges;
            else if (rising)
                *out_trigger = dmgpio_int_trigger_rising_edge;
            else if (falling)
                *out_trigger = dmgpio_int_trigger_falling_edge;
            else
                *out_trigger = dmgpio_int_trigger_off;
        }
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
    s_pins_used[port] &= ~pins;
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
    /* Use BSRR for atomic set/reset: upper 16 bits reset, lower 16 set. */
    uint32_t set_bits   = (uint32_t)data  & (uint32_t)pins;
    uint32_t reset_bits = (~(uint32_t)data & (uint32_t)pins) << 16U;
    STM32_GPIO(port)->BSRR = set_bits | reset_bits;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_data,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t *out_data ))
{
    if (!is_valid_port(port) || out_data == NULL) return -1;
    *out_data = (dmgpio_pins_mask_t)(STM32_GPIO(port)->IDR & (uint32_t)pins);
    return 0;
}

/* ======================================================================
 *  Pin state operations (no argument checking – caller must validate)
 * ====================================================================== */

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_high_state_pins,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return 0U;
    return (dmgpio_pins_mask_t)(STM32_GPIO(port)->IDR & (uint32_t)pins);
}

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_low_state_pins,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return 0U;
    return (dmgpio_pins_mask_t)(~STM32_GPIO(port)->IDR & (uint32_t)pins);
}

dmod_dmgpio_port_api_declaration(1.0, void, _set_pins_state,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_state_t state ))
{
    if (!is_valid_port(port)) return;
    if (state == dmgpio_pins_state_all_high)
        STM32_GPIO(port)->BSRR = (uint32_t)pins;          /* set */
    else
        STM32_GPIO(port)->BSRR = (uint32_t)pins << 16U;   /* reset */
}

dmod_dmgpio_port_api_declaration(1.0, void, _toggle_pins_state,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (!is_valid_port(port)) return;
    volatile stm32_gpio_t *gpio = STM32_GPIO(port);
    uint32_t current_high = gpio->ODR & (uint32_t)pins;
    /* Set pins that are currently low, reset pins that are currently high. */
    gpio->BSRR = ((uint32_t)pins & ~current_high) | (current_high << 16U);
}

/* ======================================================================
 *  EXTI interrupt common handler
 * ====================================================================== */

void stm32_gpio_exti_irq_handler(uint32_t exti_lines)
{
    volatile stm32_exti_t *exti = STM32_EXTI;
    uint32_t pending = exti->PR & exti_lines;
    exti->PR = pending; /* Writing 1 clears the pending bit. */

    if (s_interrupt_handler == NULL || pending == 0U) return;

    for (int pin = 0; pin < 16; pin++)
    {
        if (!(pending & (1U << (uint32_t)pin))) continue;

        /* Determine which GPIO port owns this EXTI line from SYSCFG_EXTICR. */
        uint32_t exticr_idx   = (uint32_t)pin / 4U;
        uint32_t exticr_shift = ((uint32_t)pin % 4U) * 4U;
        dmgpio_port_t port = (dmgpio_port_t)
            ((STM32_SYSCFG_EXTICR[exticr_idx] >> exticr_shift) & 0xFU);

        s_interrupt_handler(port, (dmgpio_pins_mask_t)(1U << pin));
    }
}

