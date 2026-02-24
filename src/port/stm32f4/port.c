#define DMOD_ENABLE_REGISTRATION    ON
#include "dmgpio_port.h"
#include "port/stm32_gpio_regs.h"

/* STM32F4 GPIO base addresses */
#define STM32F4_GPIOA_BASE  0x40020000U
#define STM32F4_GPIOB_BASE  0x40020400U
#define STM32F4_GPIOC_BASE  0x40020800U
#define STM32F4_GPIOD_BASE  0x40020C00U
#define STM32F4_GPIOE_BASE  0x40021000U
#define STM32F4_GPIOF_BASE  0x40021400U
#define STM32F4_GPIOG_BASE  0x40021800U
#define STM32F4_GPIOH_BASE  0x40021C00U
#define STM32F4_GPIOI_BASE  0x40022000U

/* STM32F4 RCC AHB1ENR register (GPIO clock enable) */
#define STM32F4_RCC_BASE    0x40023800U
#define STM32F4_RCC_AHB1ENR (*(volatile uint32_t *)(STM32F4_RCC_BASE + 0x30U))

/* Number of supported GPIO ports */
#define STM32F4_GPIO_PORT_COUNT 9U

/* GPIO base address lookup table */
static const uint32_t gpio_base_addresses[STM32F4_GPIO_PORT_COUNT] = {
    STM32F4_GPIOA_BASE,
    STM32F4_GPIOB_BASE,
    STM32F4_GPIOC_BASE,
    STM32F4_GPIOD_BASE,
    STM32F4_GPIOE_BASE,
    STM32F4_GPIOF_BASE,
    STM32F4_GPIOG_BASE,
    STM32F4_GPIOH_BASE,
    STM32F4_GPIOI_BASE,
};

/**
 * @brief Get GPIO peripheral pointer for a given port index
 *
 * @param port Port index (0=GPIOA, 1=GPIOB, ...)
 *
 * @return GPIO_TypeDef* Pointer to GPIO registers, or NULL if invalid
 */
static GPIO_TypeDef* get_gpio(dmgpio_port_t port)
{
    if (port >= STM32F4_GPIO_PORT_COUNT)
    {
        return NULL;
    }
    return (GPIO_TypeDef*)gpio_base_addresses[port];
}

/**
 * @brief Enable GPIO port clock
 *
 * @param port Port index (0=GPIOA, ...)
 */
static void enable_gpio_clock(dmgpio_port_t port)
{
    if (port < STM32F4_GPIO_PORT_COUNT)
    {
        STM32F4_RCC_AHB1ENR |= (1U << port);
        /* Short delay to allow clock to stabilize */
        (void)STM32F4_RCC_AHB1ENR;
    }
}

/**
 * @brief Initialize the DMOD module
 *
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 *
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("dmgpio port module initialized (STM32F4)\n");
    return 0;
}

/**
 * @brief Deinitialize the DMOD module
 *
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    Dmod_Printf("dmgpio port module deinitialized (STM32F4)\n");
    return 0;
}

/**
 * @brief Initialize a GPIO pin
 *
 * @param port GPIO port index (0=GPIOA, 1=GPIOB, ...)
 * @param pin GPIO pin number (0-15)
 * @param mode Pin mode
 * @param pull Pull resistor configuration
 * @param speed Output speed
 * @param alternate Alternate function number (0-15)
 *
 * @return int 0 on success, non-zero on failure
 */
dmod_dmgpio_port_api_declaration(1.0, int, _init, ( dmgpio_port_t port, dmgpio_pin_t pin, dmgpio_mode_t mode, dmgpio_pull_t pull, dmgpio_speed_t speed, uint8_t alternate ))
{
    GPIO_TypeDef* gpio = get_gpio(port);
    if (gpio == NULL || pin > 15U)
    {
        return -1;
    }

    enable_gpio_clock(port);

    /* Configure MODER */
    uint32_t moder_val;
    switch (mode)
    {
        case dmgpio_mode_output_pp:
        case dmgpio_mode_output_od:
            moder_val = GPIO_MODER_OUTPUT;
            break;
        case dmgpio_mode_af_pp:
        case dmgpio_mode_af_od:
            moder_val = GPIO_MODER_AF;
            break;
        case dmgpio_mode_analog:
            moder_val = GPIO_MODER_ANALOG;
            break;
        case dmgpio_mode_input:
        default:
            moder_val = GPIO_MODER_INPUT;
            break;
    }
    gpio->MODER = (gpio->MODER & ~(0x3U << (pin * 2U))) | (moder_val << (pin * 2U));

    /* Configure OTYPER */
    if (mode == dmgpio_mode_output_od || mode == dmgpio_mode_af_od)
    {
        gpio->OTYPER |= GPIO_OTYPER_OD << pin;
    }
    else
    {
        gpio->OTYPER &= ~(GPIO_OTYPER_OD << pin);
    }

    /* Configure OSPEEDR */
    uint32_t speed_val;
    switch (speed)
    {
        case dmgpio_speed_medium:     speed_val = GPIO_OSPEEDR_MEDIUM;    break;
        case dmgpio_speed_high:       speed_val = GPIO_OSPEEDR_HIGH;      break;
        case dmgpio_speed_very_high:  speed_val = GPIO_OSPEEDR_VERY_HIGH; break;
        case dmgpio_speed_low:
        default:                      speed_val = GPIO_OSPEEDR_LOW;       break;
    }
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(0x3U << (pin * 2U))) | (speed_val << (pin * 2U));

    /* Configure PUPDR */
    uint32_t pupdr_val;
    switch (pull)
    {
        case dmgpio_pull_up:    pupdr_val = GPIO_PUPDR_UP;   break;
        case dmgpio_pull_down:  pupdr_val = GPIO_PUPDR_DOWN; break;
        case dmgpio_pull_none:
        default:                pupdr_val = GPIO_PUPDR_NONE; break;
    }
    gpio->PUPDR = (gpio->PUPDR & ~(0x3U << (pin * 2U))) | (pupdr_val << (pin * 2U));

    /* Configure alternate function */
    if (mode == dmgpio_mode_af_pp || mode == dmgpio_mode_af_od)
    {
        uint32_t afr_idx = pin / 8U;
        uint32_t afr_pos = (pin % 8U) * 4U;
        gpio->AFR[afr_idx] = (gpio->AFR[afr_idx] & ~(0xFU << afr_pos)) | ((uint32_t)alternate << afr_pos);
    }

    return 0;
}

/**
 * @brief Deinitialize a GPIO pin (reset to input/no pull)
 *
 * @param port GPIO port index (0=GPIOA, 1=GPIOB, ...)
 * @param pin GPIO pin number (0-15)
 *
 * @return int 0 on success, non-zero on failure
 */
dmod_dmgpio_port_api_declaration(1.0, int, _deinit, ( dmgpio_port_t port, dmgpio_pin_t pin ))
{
    GPIO_TypeDef* gpio = get_gpio(port);
    if (gpio == NULL || pin > 15U)
    {
        return -1;
    }

    /* Reset to input mode, no pull, low speed */
    gpio->MODER   &= ~(0x3U << (pin * 2U));
    gpio->OTYPER  &= ~(1U << pin);
    gpio->OSPEEDR &= ~(0x3U << (pin * 2U));
    gpio->PUPDR   &= ~(0x3U << (pin * 2U));

    return 0;
}

/**
 * @brief Read the current state of a GPIO pin
 *
 * @param port GPIO port index (0=GPIOA, 1=GPIOB, ...)
 * @param pin GPIO pin number (0-15)
 *
 * @return dmgpio_pin_state_t Current pin state
 */
dmod_dmgpio_port_api_declaration(1.0, dmgpio_pin_state_t, _read_pin, ( dmgpio_port_t port, dmgpio_pin_t pin ))
{
    GPIO_TypeDef* gpio = get_gpio(port);
    if (gpio == NULL || pin > 15U)
    {
        return dmgpio_pin_reset;
    }
    return (gpio->IDR & GPIO_PIN_MASK(pin)) ? dmgpio_pin_set : dmgpio_pin_reset;
}

/**
 * @brief Write a state to a GPIO pin
 *
 * @param port GPIO port index (0=GPIOA, 1=GPIOB, ...)
 * @param pin GPIO pin number (0-15)
 * @param state Pin state to write
 */
dmod_dmgpio_port_api_declaration(1.0, void, _write_pin, ( dmgpio_port_t port, dmgpio_pin_t pin, dmgpio_pin_state_t state ))
{
    GPIO_TypeDef* gpio = get_gpio(port);
    if (gpio == NULL || pin > 15U)
    {
        return;
    }

    if (state == dmgpio_pin_set)
    {
        gpio->BSRR = GPIO_BSRR_SET(pin);
    }
    else
    {
        gpio->BSRR = GPIO_BSRR_RESET(pin);
    }
}

/**
 * @brief Toggle the state of a GPIO pin
 *
 * @param port GPIO port index (0=GPIOA, 1=GPIOB, ...)
 * @param pin GPIO pin number (0-15)
 */
dmod_dmgpio_port_api_declaration(1.0, void, _toggle_pin, ( dmgpio_port_t port, dmgpio_pin_t pin ))
{
    GPIO_TypeDef* gpio = get_gpio(port);
    if (gpio == NULL || pin > 15U)
    {
        return;
    }
    gpio->ODR ^= GPIO_PIN_MASK(pin);
}
