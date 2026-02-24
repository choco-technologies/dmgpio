#define DMOD_ENABLE_REGISTRATION    ON
#include "dmgpio_port.h"
#include "port/stm32_gpio_regs.h"
#include <stddef.h>
#include <string.h>

/* STM32F7 GPIO base addresses */
#define STM32F7_GPIOA_BASE  0x40020000U
#define STM32F7_GPIOB_BASE  0x40020400U
#define STM32F7_GPIOC_BASE  0x40020800U
#define STM32F7_GPIOD_BASE  0x40020C00U
#define STM32F7_GPIOE_BASE  0x40021000U
#define STM32F7_GPIOF_BASE  0x40021400U
#define STM32F7_GPIOG_BASE  0x40021800U
#define STM32F7_GPIOH_BASE  0x40021C00U
#define STM32F7_GPIOI_BASE  0x40022000U
#define STM32F7_GPIOJ_BASE  0x40022400U
#define STM32F7_GPIOK_BASE  0x40022800U

#define STM32F7_RCC_BASE      0x40023800U
#define STM32F7_RCC_AHB1ENR   (*(volatile uint32_t *)(STM32F7_RCC_BASE + 0x30U))
#define STM32F7_RCC_APB2ENR   (*(volatile uint32_t *)(STM32F7_RCC_BASE + 0x44U))
#define STM32F7_SYSCFG_APB2BIT  14U  /* SYSCFGEN in RCC_APB2ENR */

#define STM32F7_GPIO_PORT_COUNT 11U

static const uint32_t gpio_base_addresses[STM32F7_GPIO_PORT_COUNT] = {
    STM32F7_GPIOA_BASE, STM32F7_GPIOB_BASE, STM32F7_GPIOC_BASE,
    STM32F7_GPIOD_BASE, STM32F7_GPIOE_BASE, STM32F7_GPIOF_BASE,
    STM32F7_GPIOG_BASE, STM32F7_GPIOH_BASE, STM32F7_GPIOI_BASE,
    STM32F7_GPIOJ_BASE, STM32F7_GPIOK_BASE,
};

/* Per-port pin usage bitmasks */
static uint16_t pin_used_mask[STM32F7_GPIO_PORT_COUNT];

static dmgpio_interrupt_handler_t interrupt_handler = NULL;

static GPIO_TypeDef *get_gpio(dmgpio_port_t port)
{
    if (port >= STM32F7_GPIO_PORT_COUNT) return NULL;
    return (GPIO_TypeDef *)gpio_base_addresses[port];
}

static void set_2bit_field_for_pins(volatile uint32_t *reg, dmgpio_pins_mask_t pins, uint32_t val)
{
    for (uint8_t bit = 0; bit < 16U; bit++)
    {
        if (pins & (1U << bit))
        {
            *reg = (*reg & ~(0x3U << (bit * 2U))) | (val << (bit * 2U));
        }
    }
}

static uint32_t get_2bit_field_first_pin(uint32_t reg, dmgpio_pins_mask_t pins)
{
    for (uint8_t bit = 0; bit < 16U; bit++)
    {
        if (pins & (1U << bit))
            return (reg >> (bit * 2U)) & 0x3U;
    }
    return 0;
}

int dmod_init(const Dmod_Config_t *Config)
{
    for (dmgpio_port_t i = 0; i < STM32F7_GPIO_PORT_COUNT; i++)
        pin_used_mask[i] = 0;
    interrupt_handler = NULL;
    Dmod_Printf("dmgpio port module initialized (STM32F7)\n");
    return 0;
}

int dmod_deinit(void)
{
    for (dmgpio_port_t i = 0; i < STM32F7_GPIO_PORT_COUNT; i++)
        pin_used_mask[i] = 0;
    interrupt_handler = NULL;
    Dmod_Printf("dmgpio port module deinitialized (STM32F7)\n");
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_driver_interrupt_handler,
    ( dmgpio_interrupt_handler_t handler ))
{
    if (interrupt_handler != NULL)
    {
        DMOD_LOG_ERROR("Interrupt handler already set\n");
        return -1;
    }
    interrupt_handler = handler;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _begin_configuration,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in begin_configuration\n");
        return -1;
    }
    /* Enable GPIO port clock */
    STM32F7_RCC_AHB1ENR |= (1U << port);
    (void)STM32F7_RCC_AHB1ENR;
    /* Enable SYSCFG clock (needed for EXTI configuration) */
    STM32F7_RCC_APB2ENR |= (1U << STM32F7_SYSCFG_APB2BIT);
    (void)STM32F7_RCC_APB2ENR;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _finish_configuration,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in finish_configuration\n");
        return -1;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_power,
    ( dmgpio_port_t port, int power_on ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT)
    {
        DMOD_LOG_ERROR("Invalid port %d in set_power\n", (int)port);
        return -1;
    }
    if (power_on)
        STM32F7_RCC_AHB1ENR |= (1U << port);
    else
        STM32F7_RCC_AHB1ENR &= ~(1U << port);
    (void)STM32F7_RCC_AHB1ENR;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _is_pin_protected,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in is_pin_protected\n");
        return -1;
    }
    return (gpio->LCKR & pins) ? 1 : 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _unlock_protection,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_protection_t protection ))
{
    if (protection != dmgpio_protection_unlock_protected_pins)
    {
        DMOD_LOG_ERROR("Protection parameter must be unlock_protected_pins\n");
        return -1;
    }
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in unlock_protection\n");
        return -1;
    }
    volatile uint32_t old_val = gpio->LCKR;
    uint32_t base = old_val & ~(uint32_t)pins;
    gpio->LCKR = (1U << 16U) | base;
    gpio->LCKR = (0U << 16U) | base;
    gpio->LCKR = (1U << 16U) | base;
    (void)gpio->LCKR;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _lock_protection,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in lock_protection\n");
        return -1;
    }
    volatile uint32_t old_val = gpio->LCKR;
    uint32_t base = old_val | (uint32_t)pins;
    gpio->LCKR = (1U << 16U) | base;
    gpio->LCKR = (0U << 16U) | base;
    gpio->LCKR = (1U << 16U) | base;
    (void)gpio->LCKR;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_mode,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t mode ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in set_mode\n");
        return -1;
    }
    uint32_t val;
    switch (mode)
    {
        case dmgpio_mode_output:    val = GPIO_MODER_OUTPUT; break;
        case dmgpio_mode_alternate: val = GPIO_MODER_AF;     break;
        default:                    val = GPIO_MODER_INPUT;  break;
    }
    set_2bit_field_for_pins(&gpio->MODER, pins, val);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_mode,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_mode_t *out_mode ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0 || out_mode == NULL)
    {
        DMOD_LOG_ERROR("Invalid arguments in read_mode\n");
        return -1;
    }
    uint32_t val = get_2bit_field_first_pin(gpio->MODER, pins);
    switch (val)
    {
        case GPIO_MODER_OUTPUT: *out_mode = dmgpio_mode_output;    break;
        case GPIO_MODER_AF:     *out_mode = dmgpio_mode_alternate; break;
        default:                *out_mode = dmgpio_mode_input;     break;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_output_circuit,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t oc ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in set_output_circuit\n");
        return -1;
    }
    if (oc == dmgpio_output_circuit_open_drain)
        gpio->OTYPER |= (uint32_t)pins;
    else
        gpio->OTYPER &= ~(uint32_t)pins;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_output_circuit,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_output_circuit_t *out_oc ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0 || out_oc == NULL)
    {
        DMOD_LOG_ERROR("Invalid arguments in read_output_circuit\n");
        return -1;
    }
    /* Report the state of the first set pin in the mask */
    uint8_t first_bit = 0;
    while (first_bit < 16U && !(pins & (1U << first_bit))) first_bit++;
    *out_oc = (gpio->OTYPER & (1U << first_bit)) ?
        dmgpio_output_circuit_open_drain : dmgpio_output_circuit_push_pull;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_speed,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t speed ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in set_speed\n");
        return -1;
    }
    uint32_t val;
    switch (speed)
    {
        case dmgpio_speed_medium:  val = GPIO_OSPEEDR_MEDIUM;    break;
        case dmgpio_speed_maximum: val = GPIO_OSPEEDR_VERY_HIGH; break;
        default:                   val = GPIO_OSPEEDR_LOW;       break;
    }
    set_2bit_field_for_pins(&gpio->OSPEEDR, pins, val);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_speed,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_speed_t *out_speed ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0 || out_speed == NULL)
    {
        DMOD_LOG_ERROR("Invalid arguments in read_speed\n");
        return -1;
    }
    uint32_t val = get_2bit_field_first_pin(gpio->OSPEEDR, pins);
    switch (val)
    {
        case GPIO_OSPEEDR_MEDIUM:    *out_speed = dmgpio_speed_medium;  break;
        case GPIO_OSPEEDR_HIGH:
        case GPIO_OSPEEDR_VERY_HIGH: *out_speed = dmgpio_speed_maximum; break;
        default:                     *out_speed = dmgpio_speed_minimum; break;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_current,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t current ))
{
    /* STM32F7 does not have a separate current register */
    (void)port; (void)pins; (void)current;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_current,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_current_t *out_current ))
{
    (void)port; (void)pins;
    if (out_current == NULL)
    {
        DMOD_LOG_ERROR("Null output pointer in read_current\n");
        return -1;
    }
    *out_current = dmgpio_current_default;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_pull,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t pull ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in set_pull\n");
        return -1;
    }
    uint32_t val;
    switch (pull)
    {
        case dmgpio_pull_up:   val = GPIO_PUPDR_UP;   break;
        case dmgpio_pull_down: val = GPIO_PUPDR_DOWN; break;
        default:               val = GPIO_PUPDR_NONE; break;
    }
    set_2bit_field_for_pins(&gpio->PUPDR, pins, val);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_pull,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pull_t *out_pull ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || pins == 0 || out_pull == NULL)
    {
        DMOD_LOG_ERROR("Invalid arguments in read_pull\n");
        return -1;
    }
    uint32_t val = get_2bit_field_first_pin(gpio->PUPDR, pins);
    switch (val)
    {
        case GPIO_PUPDR_UP:   *out_pull = dmgpio_pull_up;      break;
        case GPIO_PUPDR_DOWN: *out_pull = dmgpio_pull_down;    break;
        default:              *out_pull = dmgpio_pull_default;  break;
    }
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_interrupt_trigger,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t trigger ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT || pins == 0)
    {
        DMOD_LOG_ERROR("Invalid port or pins in set_interrupt_trigger\n");
        return -1;
    }

    if (trigger == dmgpio_int_trigger_off)
    {
        EXTI_IMR  &= ~(uint32_t)pins;
        EXTI_RTSR &= ~(uint32_t)pins;
        EXTI_FTSR &= ~(uint32_t)pins;
        return 0;
    }

    /* Connect GPIO port to EXTI lines via SYSCFG_EXTICRx */
    for (uint8_t bit = 0; bit < 16U; bit++)
    {
        if (pins & (1U << bit))
        {
            uint8_t reg_idx = bit / 4U;              /* EXTICR register index (0-3) */
            uint8_t reg_pos = (bit % 4U) * 4U;       /* each EXTI line uses 4 bits: positions 0,4,8,12 */
            SYSCFG_EXTICR(reg_idx) = (SYSCFG_EXTICR(reg_idx) & ~(0xFU << reg_pos))
                                     | ((uint32_t)port << reg_pos);
        }
    }

    if (trigger & dmgpio_int_trigger_rising_edge)
    {
        EXTI_RTSR |= (uint32_t)pins;
        EXTI_IMR  |= (uint32_t)pins;
    }
    if (trigger & dmgpio_int_trigger_falling_edge)
    {
        EXTI_FTSR |= (uint32_t)pins;
        EXTI_IMR  |= (uint32_t)pins;
    }

    if ((trigger & dmgpio_int_trigger_high_level) || (trigger & dmgpio_int_trigger_low_level))
    {
        DMOD_LOG_ERROR("Level-triggered interrupts are not supported on STM32F7\n");
        return -1;
    }

    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_interrupt_trigger,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_int_trigger_t *out_trigger ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT || pins == 0 || out_trigger == NULL)
    {
        DMOD_LOG_ERROR("Invalid arguments in read_interrupt_trigger\n");
        return -1;
    }
    dmgpio_int_trigger_t trig = dmgpio_int_trigger_off;
    /* Read trigger state for the first set pin in the mask */
    uint32_t first_pin_bit = 0;
    while (first_pin_bit < 16U && !(pins & (1U << first_pin_bit))) first_pin_bit++;
    if (EXTI_IMR & (1U << first_pin_bit))
    {
        if (EXTI_RTSR & (1U << first_pin_bit)) trig |= dmgpio_int_trigger_rising_edge;
        if (EXTI_FTSR & (1U << first_pin_bit)) trig |= dmgpio_int_trigger_falling_edge;
    }
    *out_trigger = trig;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_pins_used,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT)
    {
        DMOD_LOG_ERROR("Invalid port %d in set_pins_used\n", (int)port);
        return -1;
    }
    pin_used_mask[port] |= pins;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _set_pins_unused,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT)
    {
        DMOD_LOG_ERROR("Invalid port %d in set_pins_unused\n", (int)port);
        return -1;
    }
    pin_used_mask[port] &= ~pins;
    EXTI_IMR  &= ~(uint32_t)pins;
    EXTI_RTSR &= ~(uint32_t)pins;
    EXTI_FTSR &= ~(uint32_t)pins;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _check_is_pin_used,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, int *out_used ))
{
    if (port >= STM32F7_GPIO_PORT_COUNT || pins == 0 || out_used == NULL)
    {
        DMOD_LOG_ERROR("Invalid arguments in check_is_pin_used\n");
        return -1;
    }
    *out_used = (pin_used_mask[port] & pins) ? 1 : 0;
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _write_data,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t data ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL)
    {
        DMOD_LOG_ERROR("Invalid port %d in write_data\n", (int)port);
        return -1;
    }
    uint32_t set_bits   = (uint32_t)(pins &  data);
    uint32_t reset_bits = (uint32_t)(pins & ~data);
    gpio->BSRR = set_bits | (reset_bits << 16U);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _read_data,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t *out_data ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL || out_data == NULL)
    {
        DMOD_LOG_ERROR("Invalid arguments in read_data\n");
        return -1;
    }
    *out_data = (dmgpio_pins_mask_t)(gpio->IDR & pins);
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_high_state_pins,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL) return 0;
    return (dmgpio_pins_mask_t)(gpio->IDR & pins);
}

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pins_mask_t, _get_low_state_pins,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL) return 0;
    return (dmgpio_pins_mask_t)(pins & ~gpio->IDR);
}

dmod_dmgpio_port_api_declaration(1.0, void, _set_pins_state,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_state_t state ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL) return;
    if (state == dmgpio_pins_state_all_high)
        gpio->BSRR = (uint32_t)pins;
    else
        gpio->BSRR = (uint32_t)pins << 16U;
}

dmod_dmgpio_port_api_declaration(1.0, void, _toggle_pins_state,
    ( dmgpio_port_t port, dmgpio_pins_mask_t pins ))
{
    GPIO_TypeDef *gpio = get_gpio(port);
    if (gpio == NULL) return;
    uint32_t odr_val    = gpio->ODR;
    uint32_t set_bits   = (~odr_val) & pins;
    uint32_t reset_bits = odr_val    & pins;
    gpio->BSRR = set_bits | (reset_bits << 16U);
}
