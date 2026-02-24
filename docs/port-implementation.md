# DMGPIO Port Implementation Guide

## Overview

This guide explains how to add GPIO support for a new microcontroller family to the DMGPIO module.

## Port Layer API

The port layer is defined in `include/dmgpio_port.h`. Each hardware-specific implementation must provide the following functions using the `dmod_dmgpio_port_api_declaration` macro:

| Function | Description |
|----------|-------------|
| `dmgpio_port_init` | Initialize a GPIO pin |
| `dmgpio_port_deinit` | Reset a GPIO pin to default state |
| `dmgpio_port_read_pin` | Read the current pin state |
| `dmgpio_port_write_pin` | Set the pin output state |
| `dmgpio_port_toggle_pin` | Toggle the pin output state |

## Adding a New Platform

### 1. Create a New Port Directory

Create a directory under `src/port/` named after your MCU series:

```
src/port/<mcu_series>/
├── config.cmake
└── port.c
```

### 2. Create `config.cmake`

This file sets the DMOD toolchain name for the target architecture.

**Example for Cortex-M4:**
```cmake
set(DMOD_TOOLS_NAME "arch/armv7/cortex-m4" CACHE STRING "Name of the tools configuration")
```

**Example for Cortex-M7:**
```cmake
set(DMOD_TOOLS_NAME "arch/armv7/cortex-m7" CACHE STRING "Name of the tools configuration")
```

### 3. Create `port.c`

Your `port.c` file must:

1. Define `DMOD_ENABLE_REGISTRATION ON` before any includes
2. Include `dmgpio_port.h`
3. Implement `dmod_init` and `dmod_deinit` for DMOD lifecycle hooks
4. Implement all five port API functions

**Template:**

```c
#define DMOD_ENABLE_REGISTRATION    ON
#include "dmgpio_port.h"

// Your hardware-specific includes and definitions here

int dmod_init(const Dmod_Config_t *Config)
{
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _init,
    ( dmgpio_port_t port, dmgpio_pin_t pin,
      dmgpio_mode_t mode, dmgpio_pull_t pull,
      dmgpio_speed_t speed, uint8_t alternate ))
{
    // Initialize GPIO registers for your hardware
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, int, _deinit,
    ( dmgpio_port_t port, dmgpio_pin_t pin ))
{
    // Reset pin to default (input, no pull)
    return 0;
}

dmod_dmgpio_port_api_declaration(1.0, dmgpio_pin_state_t, _read_pin,
    ( dmgpio_port_t port, dmgpio_pin_t pin ))
{
    // Return dmgpio_pin_set or dmgpio_pin_reset
    return dmgpio_pin_reset;
}

dmod_dmgpio_port_api_declaration(1.0, void, _write_pin,
    ( dmgpio_port_t port, dmgpio_pin_t pin, dmgpio_pin_state_t state ))
{
    // Set or reset the pin
}

dmod_dmgpio_port_api_declaration(1.0, void, _toggle_pin,
    ( dmgpio_port_t port, dmgpio_pin_t pin ))
{
    // Toggle the pin output
}
```

### 4. Build and Test

Build with your new MCU series:

```bash
mkdir build_<mcu_series>
cd build_<mcu_series>
cmake .. -DDMGPIO_MCU_SERIES=<mcu_series>
cmake --build .
```

## STM32 Implementation Notes

### GPIO Register Layout

STM32F4 and STM32F7 share the same GPIO register layout (see `include/port/stm32_gpio_regs.h`):

| Register | Description |
|----------|-------------|
| `MODER` | Mode register (2 bits per pin: 00=input, 01=output, 10=AF, 11=analog) |
| `OTYPER` | Output type (0=push-pull, 1=open-drain) |
| `OSPEEDR` | Output speed (2 bits per pin) |
| `PUPDR` | Pull-up/pull-down (2 bits per pin: 00=none, 01=up, 10=down) |
| `IDR` | Input data register |
| `ODR` | Output data register |
| `BSRR` | Bit set/reset register (atomic operation) |
| `AFR[2]` | Alternate function registers (4 bits per pin) |

### Clock Enable

Before using any GPIO port, its AHB1 clock must be enabled in `RCC->AHB1ENR`. The STM32 port implementations do this automatically in `dmgpio_port_init`.

### Atomic Operations

Use the `BSRR` register for atomic pin set/reset operations to avoid read-modify-write race conditions.

## Port Base Address

Port base addresses are typically consecutive from `GPIOA_BASE`:

| Port | Offset |
|------|--------|
| GPIOA | `+0x000` |
| GPIOB | `+0x400` |
| GPIOC | `+0x800` |
| ... | ... |

Check the reference manual for the exact base address of your MCU.
