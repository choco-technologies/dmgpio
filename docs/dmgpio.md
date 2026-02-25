# DMGPIO Module Overview

## Introduction

DMGPIO is a DMOD (Dynamic Modular System) module that provides a hardware-independent GPIO (General Purpose Input/Output) driver interface for embedded microcontrollers. It follows the DMDRVI (DMOD Driver Interface) pattern and integrates with the DMINI configuration parser.

## Architecture

```
┌─────────────────────────────────────────┐
│           Application Code              │
└────────────────┬────────────────────────┘
                 │  DMDRVI API
┌────────────────▼────────────────────────┐
│              dmgpio                     │
│  (Hardware-independent GPIO driver)     │
│                                         │
│  - Configuration parsing (via dmini)    │
│  - DMDRVI interface implementation      │
│  - IOCTL command handling               │
└────────────────┬────────────────────────┘
                 │  dmgpio_port API
┌────────────────▼────────────────────────┐
│           dmgpio_port                   │
│  (Hardware-specific implementation)     │
│                                         │
│  - STM32F4 port                         │
│  - STM32F7 port                         │
└─────────────────────────────────────────┘
```

## Key Concepts

### GPIO Pin Configuration

Each GPIO device instance represents a single GPIO pin with its configuration:

- **Port**: The GPIO port (A–K, depending on MCU)
- **Pin**: The pin number within the port (0–15)
- **Mode**: Operating mode (input, output_pp, output_od, af_pp, af_od, analog)
- **Pull**: Internal pull resistor (none, up, down)
- **Speed**: Output slew rate (low, medium, high, very_high)
- **Alternate**: Alternate function number for AF modes (0–15)

### DMDRVI Integration

DMGPIO implements the full DMDRVI interface:

| Function | Description |
|----------|-------------|
| `dmgpio_dmdrvi_create` | Create a GPIO device context from INI configuration |
| `dmgpio_dmdrvi_free` | Free the device context and deinitialize the pin |
| `dmgpio_dmdrvi_open` | Open a handle to the GPIO device |
| `dmgpio_dmdrvi_close` | Close the device handle |
| `dmgpio_dmdrvi_read` | Read high-state pin bitmask as hex string, e.g. `"0x000A"` |
| `dmgpio_dmdrvi_write` | Write pin states via decimal/hex bitmask string, e.g. `"0x000A"` or `"10"` |
| `dmgpio_dmdrvi_ioctl` | Control and query pin state and configuration |
| `dmgpio_dmdrvi_flush` | Flush (no-op for GPIO) |
| `dmgpio_dmdrvi_stat` | Get device statistics |

### IOCTL Commands

See `dmgpio_ioctl_cmd_t` in `dmgpio.h` for a full list of supported IOCTL commands.

## Module Files

```
dmgpio/
├── configs/           # Pre-configured board and MCU configurations
│   ├── board/        # Board-specific configurations
│   └── mcu/          # MCU-specific configurations
├── docs/              # Documentation (markdown format)
├── examples/          # Example configurations
├── include/           # Public headers
│   ├── dmgpio.h      # Main API (types, enums, IOCTL commands)
│   ├── dmgpio_port.h # Port layer API
│   └── port/         # Port-specific headers
│       └── stm32_gpio_regs.h  # STM32 GPIO register layout
├── src/
│   ├── dmgpio.c      # Core implementation
│   └── port/         # Hardware-specific implementations
│       ├── stm32f4/  # STM32F4 port
│       └── stm32f7/  # STM32F7 port
├── CMakeLists.txt    # Build configuration
├── dmgpio.dmr        # DMOD resource file
└── manifest.dmm      # DMOD manifest
```
