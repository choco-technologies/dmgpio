# DMGPIO - DMOD GPIO Driver Module

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A DMOD (Dynamic Modular System) module for configuring and managing GPIO (General Purpose Input/Output) pins on embedded microcontrollers.

## Features

- **Multiple Pin Modes**: Input, output push-pull, output open-drain, alternate function, and analog
- **Pull Resistor Control**: No pull, pull-up, and pull-down configurations
- **Output Speed Control**: Low, medium, high, and very high slew rates
- **Alternate Function Support**: Full alternate function configuration for peripherals (UART, SPI, I2C, etc.)
- **Dynamic Reconfiguration**: Runtime pin configuration updates via IOCTL
- **Interrupt Handler Binding via dmhaman**: Specify a [dmhaman](https://github.com/choco-technologies/dmhaman)-registered handler name directly in the INI config — no `ioctl` calls needed at runtime
- **Hardware Abstraction**: Platform-independent API with hardware-specific implementations
- **DMDRVI Integration**: Full DMOD driver interface implementation
- **STM32 Support**: STM32F4 and STM32F7 families currently supported
- **Extensible**: Easy to add support for additional microcontroller families

## Quick Start

### Installation

Using `dmf-get` from the DMOD release package:

```bash
dmf-get install dmgpio
```

Or install with a pre-configured setup for your board:

```bash
# Create a dependencies file (deps.dmd)
echo "dmgpio@latest board/nucleo-f767zi.ini" > deps.dmd

# Install with configuration
dmf-get -d deps.dmd --config-dir ./config
```

The module includes ready-to-use configuration files for many popular STM32 boards and MCUs. See [`configs/README.md`](configs/README.md) for the complete list.

### Basic Usage

1. **Create a configuration file** (`config.ini`):

```ini
[dmgpio]
port=A
pin=5
mode=output_pp
pull=none
speed=low
alternate=0
```

2. **Use in your code**:

```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"

// Load configuration and create device
dmini_context_t config = dmini_load("config.ini");
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t gpio_ctx = dmgpio_dmdrvi_create(config, &dev_num);

// Open and use the GPIO device
void* handle = dmgpio_dmdrvi_open(gpio_ctx, DMDRVI_O_RDWR);

// Set pin high
dmgpio_pin_state_t state = dmgpio_pin_set;
dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_set_state, &state);

// Toggle the pin
dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_toggle, NULL);

// Cleanup
dmgpio_dmdrvi_close(gpio_ctx, handle);
dmgpio_dmdrvi_free(gpio_ctx);
dmini_free(config);
```

## Building

### Prerequisites

- CMake 3.18 or higher
- ARM GCC toolchain (for embedded targets)
- DMOD framework (automatically fetched)

### Build Commands

```bash
# Configure for STM32F4
cmake -DDMGPIO_MCU_SERIES=stm32f4 -B build

# Configure for STM32F7
cmake -DDMGPIO_MCU_SERIES=stm32f7 -B build

# Build
cmake --build build
```

## Documentation

Comprehensive documentation is available in the `docs/` directory:

- **[dmgpio.md](docs/dmgpio.md)** - Module overview and architecture
- **[api-reference.md](docs/api-reference.md)** - Complete API documentation
- **[configuration.md](docs/configuration.md)** - Configuration guide with examples
- **[port-implementation.md](docs/port-implementation.md)** - Guide for adding hardware support
- **[examples.md](docs/examples.md)** - Usage examples

View documentation using `dmf-man`:

```bash
dmf-man dmgpio          # Main documentation
dmf-man dmgpio api      # API reference
dmf-man dmgpio config   # Configuration guide
dmf-man dmgpio port     # Port implementation guide
```

## Supported Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| STM32F4  | ✅ Supported | Full GPIO configuration |
| STM32F7  | ✅ Supported | Full GPIO configuration |
| Other STM32 | 🔧 In Progress | Easy to add via register-level abstraction |
| Other MCUs | 📋 Planned | Contributions welcome |

## Pre-configured Boards and MCUs

The module includes ready-to-use configuration files for popular development boards and microcontrollers:

**Development Boards:**
- STM32F4DISCOVERY, STM32F429I-DISCOVERY
- NUCLEO-F401RE, NUCLEO-F411RE, NUCLEO-F446RE
- STM32F746G-DISCO, NUCLEO-F767ZI, STM32F769I-DISCOVERY

**Microcontrollers:**
- STM32F4 series: F401RE, F405RG, F407VG, F411RE, F429ZI, F439ZI, F446RE, F469NI
- STM32F7 series: F722RE, F746ZG, F767ZI, F769NI

For a complete list and usage instructions, see [`configs/README.md`](configs/README.md).

## Configuration Examples

### Output Pin (LED)

```ini
[dmgpio]
port=A
pin=5
mode=output_pp
pull=none
speed=low
alternate=0
```

### Input Pin (Button with pull-down)

```ini
[dmgpio]
port=C
pin=13
mode=input
pull=down
speed=low
alternate=0
```

### Alternate Function Pin (UART TX)

```ini
[dmgpio]
port=A
pin=9
mode=af_pp
pull=none
speed=very_high
alternate=7
```

### Input Pin with dmhaman Interrupt Handler

Bind an interrupt directly in the config file — no `ioctl` calls needed at runtime.  Any module that registers the same name with [dmhaman](https://github.com/choco-technologies/dmhaman) will receive the interrupt notification:

```ini
[dmgpio]
pin=PC13
mode=input
pull=up
interrupt_trigger=falling_edge
interrupt_handler=spi.cs1
```

Subscribe from any module:

```c
// params is dmgpio_interrupt_params_t * (port, pins, state)
dmhaman_register_handler("spi.cs1", my_callback, my_ctx);
```

## Development

### Project Structure

```
dmgpio/
├── configs/           # Pre-configured board and MCU configurations
│   ├── board/        # Board-specific configurations
│   └── mcu/          # MCU-specific configurations
├── docs/              # Documentation (markdown format)
├── examples/          # Example configurations
├── include/           # Public headers
│   ├── dmgpio.h      # Main API
│   ├── dmgpio_port.h # Port layer API
│   └── port/         # Port-specific headers
├── src/
│   ├── dmgpio.c      # Core implementation
│   └── port/         # Hardware-specific implementations
│       ├── stm32f4/  # STM32F4 port
│       └── stm32f7/  # STM32F7 port
├── CMakeLists.txt    # Build configuration
├── dmgpio.dmr        # DMOD resource file
└── manifest.dmm      # DMOD manifest
```

### Adding New Platform Support

See [Port Implementation Guide](docs/port-implementation.md) for detailed instructions on adding support for new microcontrollers.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Authors

- Patryk Kubiak - Initial work

## Related Projects

- [DMOD](https://github.com/choco-technologies/dmod) - Dynamic Modular System framework
- [DMINI](https://github.com/choco-technologies/dmini) - INI configuration parser for DMOD
- [DMDRVI](https://github.com/choco-technologies/dmdrvi) - DMOD Driver Interface
- [DMCLK](https://github.com/choco-technologies/dmclk) - DMOD Clock Configuration Module
- [DMHAMAN](https://github.com/choco-technologies/dmhaman) - DMOD Handler Manager (named interrupt/event handler registry)

## Support

For issues, questions, or contributions:

- Open an issue on GitHub
- Check the documentation in `docs/`
- Use `dmf-man dmgpio` for command-line help
