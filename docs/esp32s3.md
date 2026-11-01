# DMGPIO on the ESP32-S3

The port (`src/port/esp32s3`) drives the GPIO matrix and the IO MUX directly. Build it with `-DDMGPIO_MCU_SERIES=esp32s3`
(tools: `arch/xtensa/esp32s3`).

## Ports and pins

The ESP32-S3 has 49 pads (GPIO0-21 and GPIO26-48). dmgpio addresses a pin by a port and a 16-bit mask, so the pads are cut
into ports of 16:

| Port | GPIO |
|------|------|
| `A` | 0-15 |
| `B` | 16-31 (22-25 do not exist) |
| `C` | 32-47 |
| `D` | 48 |

`pin=PB3` is GPIO19, `pin=PA0` is GPIO0 (a strapping pin). A pin that does not exist is refused.

## What the settings mean

| dmgpio | ESP32-S3 |
|--------|----------|
| `mode=input` | IO MUX function 1 (GPIO), input enabled, output disabled |
| `mode=output` | IO MUX function 1, the pad is driven by the GPIO matrix (`GPIO_FUNCn_OUT_SEL_CFG` = 256), output enabled; the input stays on, so the level can be read back |
| `mode=alternate` + `alternate=N` | IO MUX function `N` (0-5), the pad is not driven by the GPIO matrix. Peripherals that are reachable through the GPIO matrix only are routed by their drivers |
| `pull=up` / `pull=down` | `FUN_PU` / `FUN_PD` of the pad |
| `current=minimum` / `medium` / `maximum` | drive strength 0 / 2 / 3 (about 5 / 20 / 40 mA) |
| `output_circuit=open_drain` | `GPIO_PINn.PAD_DRIVER` |
| `speed=...` | not supported (`-ENOTSUP`): the pads have no slew rate setting; `default` is accepted |
| interrupts | not supported yet (`-ENOTSUP`): the dispatcher of the interrupts of the Xtensa ports is not there |

## Protected pins

The in-package flash and the PSRAM of the ESP32-S3FN16R8 (GPIO26-37) and the USB D-/D+ (GPIO19, GPIO20: the debug probe and the
console of the board) need `protection=unlock` in the configuration of a pin.
