# DMGPIO Configuration Guide

## Overview

DMGPIO reads its pin configuration from an INI file using the DMINI module. The configuration specifies the GPIO port, pin number, and electrical characteristics.

## Configuration File Format

The configuration uses a `[dmgpio]` section:

```ini
[dmgpio]
port=A
pin=5
mode=output_pp
pull=none
speed=low
alternate=0
```

## Parameters

### `port`

The GPIO port letter.

| Value | Description |
|-------|-------------|
| `A`–`K` | GPIO port (range depends on MCU) |

**Example:** `port=A` (GPIOA)

---

### `pin`

The pin number within the port.

| Value | Description |
|-------|-------------|
| `0`–`15` | GPIO pin number |

**Example:** `pin=5` (pin 5)

---

### `mode`

The operating mode of the GPIO pin.

| Value | Description | Use Case |
|-------|-------------|----------|
| `input` | Digital input | Buttons, signals |
| `output_pp` | Push-pull output | LEDs, digital outputs |
| `output_od` | Open-drain output | I2C-style wired-AND |
| `af_pp` | Alternate function push-pull | UART TX, SPI, etc. |
| `af_od` | Alternate function open-drain | I2C SDA/SCL |
| `analog` | Analog mode | ADC/DAC pins |

**Example:** `mode=output_pp`

---

### `pull`

Internal pull resistor configuration.

| Value | Description |
|-------|-------------|
| `none` | Floating (no pull) |
| `up` | Pull-up resistor (~40 kΩ typical) |
| `down` | Pull-down resistor (~40 kΩ typical) |

**Example:** `pull=none`

---

### `speed`

Output slew rate. Only relevant for output and alternate function modes.

| Value | Typical Max Frequency |
|-------|-----------------------|
| `low` | ~2 MHz |
| `medium` | ~25 MHz |
| `high` | ~50 MHz |
| `very_high` | ~100 MHz |

**Example:** `speed=low`

---

### `alternate`

Alternate function number (0–15). Only used when `mode` is `af_pp` or `af_od`. Refer to the MCU datasheet for the correct alternate function number.

**Example:** `alternate=7` (USART on STM32F4/F7)

---

## Common Configuration Examples

### User LED (Output)

```ini
[dmgpio]
port=A
pin=5
mode=output_pp
pull=none
speed=low
alternate=0
```

### User Button (Input with Pull-Down)

```ini
[dmgpio]
port=C
pin=13
mode=input
pull=down
speed=low
alternate=0
```

### UART TX Pin (Alternate Function)

```ini
[dmgpio]
port=A
pin=9
mode=af_pp
pull=none
speed=very_high
alternate=7
```

### I2C SDA Pin (Open-Drain Alternate Function)

```ini
[dmgpio]
port=B
pin=7
mode=af_od
pull=up
speed=high
alternate=4
```

### ADC Input (Analog Mode)

```ini
[dmgpio]
port=A
pin=0
mode=analog
pull=none
speed=low
alternate=0
```

## Pre-Configured Files

The `configs/` directory contains ready-to-use INI files for popular boards and MCUs.
See [`configs/README.md`](../configs/README.md) for the full list.

## Loading Configuration at Runtime

```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"

// Load configuration file
dmini_context_t config = dmini_load("config.ini");
if (config == NULL) {
    // handle error
}

// Create GPIO device
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t gpio_ctx = dmgpio_dmdrvi_create(config, &dev_num);
if (gpio_ctx == NULL) {
    dmini_free(config);
    // handle error
}

// Use the device...

// Cleanup
dmgpio_dmdrvi_free(gpio_ctx);
dmini_free(config);
```

## Runtime Reconfiguration

You can update pin settings at runtime using IOCTL commands:

```c
// Change pin mode to input
dmgpio_mode_t new_mode = dmgpio_mode_input;
dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_set_mode, &new_mode);

// Change pull resistor
dmgpio_pull_t new_pull = dmgpio_pull_up;
dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_set_pull, &new_pull);

// Reapply configuration to hardware
dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_reconfigure, NULL);
```
