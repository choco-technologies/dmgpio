# DMGPIO Configuration Files

This directory contains default GPIO pin configuration files for various STM32 development boards and microcontrollers. These configuration files can be copied to your project during module installation using the `dmf-get` tool with the `--config-dir` option.

## Directory Structure

- **`board/`** - Configuration files for specific development boards
- **`mcu/`** - Configuration files for specific microcontrollers

## Usage

When installing the dmgpio module, you can specify a configuration file in your `.dmd` dependencies file:

```dmd
# Install dmgpio with configuration for STM32F746G-DISCO board
dmgpio@1.0 board/stm32f746g-disco.ini

# Install dmgpio with configuration for STM32F407VG MCU
dmgpio@1.0 mcu/stm32f407vg.ini
```

Then install with:

```bash
dmf-get -d project-deps.dmd --config-dir ./config
```

This will copy the specified configuration file to your project's config directory.

## Board Configurations

Board configurations are pre-set with the pin for the user LED of each development board:

| Board | MCU | LED Pin | File |
|-------|-----|---------|------|
| STM32F4DISCOVERY | STM32F407VG | PD12 (Green LED) | `board/stm32f4-discovery.ini` |
| STM32F429I-DISCOVERY | STM32F429ZI | PG13 (Green LED) | `board/stm32f429i-discovery.ini` |
| NUCLEO-F401RE | STM32F401RE | PA5 (User LED LD2) | `board/nucleo-f401re.ini` |
| NUCLEO-F411RE | STM32F411RE | PA5 (User LED LD2) | `board/nucleo-f411re.ini` |
| NUCLEO-F446RE | STM32F446RE | PA5 (User LED LD2) | `board/nucleo-f446re.ini` |
| STM32F746G-DISCO | STM32F746NG | PI1 (Green LED LD1) | `board/stm32f746g-disco.ini` |
| NUCLEO-F767ZI | STM32F767ZI | PB0 (Green LED LD1) | `board/nucleo-f767zi.ini` |
| STM32F769I-DISCOVERY | STM32F769NI | PJ5 (Green LED LD2) | `board/stm32f769i-discovery.ini` |

## MCU Configurations

MCU configurations provide default pin settings for specific microcontrollers:

### STM32F4 Series

| MCU | Default Pin | File |
|-----|-------------|------|
| STM32F401RE | PA5 | `mcu/stm32f401re.ini` |
| STM32F405RG | PA5 | `mcu/stm32f405rg.ini` |
| STM32F407VG | PD12 | `mcu/stm32f407vg.ini` |
| STM32F411RE | PA5 | `mcu/stm32f411re.ini` |
| STM32F429ZI | PG13 | `mcu/stm32f429zi.ini` |
| STM32F439ZI | PG13 | `mcu/stm32f439zi.ini` |
| STM32F446RE | PA5 | `mcu/stm32f446re.ini` |
| STM32F469NI | PG13 | `mcu/stm32f469ni.ini` |

### STM32F7 Series

| MCU | Default Pin | File |
|-----|-------------|------|
| STM32F722RE | PB0 | `mcu/stm32f722re.ini` |
| STM32F746ZG | PB0 | `mcu/stm32f746zg.ini` |
| STM32F767ZI | PB0 | `mcu/stm32f767zi.ini` |
| STM32F769NI | PJ5 | `mcu/stm32f769ni.ini` |

## Configuration Format

All configuration files use the INI format with the `[dmgpio]` section:

```ini
[dmgpio]
port=A          # GPIO port letter (A-K)
pin=5           # Pin number (0-15)
mode=output_pp  # Pin mode (see below)
pull=none       # Pull resistor (none, up, down)
speed=low       # Output speed (low, medium, high, very_high)
alternate=0     # Alternate function number (0-15, for AF modes)
```

### Parameters

- **port**: GPIO port letter
  - `A` through `K` depending on MCU

- **pin**: GPIO pin number (0–15)

- **mode**: Pin operating mode
  - `input` - Digital input
  - `output_pp` - Digital output, push-pull
  - `output_od` - Digital output, open-drain
  - `af_pp` - Alternate function, push-pull
  - `af_od` - Alternate function, open-drain
  - `analog` - Analog mode (ADC/DAC)

- **pull**: Internal pull resistor
  - `none` - No pull resistor
  - `up` - Pull-up resistor
  - `down` - Pull-down resistor

- **speed**: Output slew rate (relevant for output and alternate function modes)
  - `low` - Low speed (~2 MHz)
  - `medium` - Medium speed (~25 MHz)
  - `high` - High speed (~50 MHz)
  - `very_high` - Very high speed (~100 MHz)

- **alternate**: Alternate function number (0–15), used only when `mode` is `af_pp` or `af_od`

## Customization

You can modify these configuration files for your specific hardware:

1. Copy the configuration file to your project
2. Adjust `port` and `pin` to match your hardware
3. Set `mode` according to the pin's purpose
4. Configure `pull`, `speed`, and `alternate` as needed

## See Also

- [DMGPIO Configuration Guide](../docs/configuration.md) - Detailed configuration documentation
- [DMR File Format](https://github.com/choco-technologies/dmod/blob/develop/docs/dmr-file-format.md) - Resource file format
- [DMD File Format](https://github.com/choco-technologies/dmod/blob/develop/docs/dmd-file-format.md) - Dependencies file format
