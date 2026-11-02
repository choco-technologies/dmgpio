#ifndef ESP32S3_GPIO_H
#define ESP32S3_GPIO_H

#include <stdint.h>

/*
 * ESP32-S3: 49 pads, GPIO0-21 and GPIO26-48 (22-25 do not exist). dmgpio addresses a pin by a port and a 16-bit mask, so the
 * pads are cut into ports of 16: port A = GPIO0-15, B = GPIO16-31, C = GPIO32-47, D = GPIO48 ("PB3" is GPIO19).
 */

/** Number of the ports (A-D) */
#define ESP32S3_GPIO_PORTS              4U
/** Pins that exist, by port (GPIO22-25 are missing, port D has GPIO48 only) */
#define ESP32S3_GPIO_VALID_PINS_A       0xFFFFU
#define ESP32S3_GPIO_VALID_PINS_B       0xFC3FU
#define ESP32S3_GPIO_VALID_PINS_C       0xFFFFU
#define ESP32S3_GPIO_VALID_PINS_D       0x0001U
/**
 * Pins that dmgpio refuses to touch without "protection=unlock": the in-package flash and the PSRAM of the ESP32-S3FN16R8
 * (GPIO26-37) and the USB D-/D+ (GPIO19, GPIO20: the debug probe and the console of the board).
 */
#define ESP32S3_GPIO_PROTECTED_PINS_B   0xFC18U
#define ESP32S3_GPIO_PROTECTED_PINS_C   0x003FU

/* GPIO matrix (DR_REG_GPIO_BASE) */
#define ESP32S3_GPIO_BASE               0x60004000UL
#define ESP32S3_GPIO_OUT_REG            (ESP32S3_GPIO_BASE + 0x04UL)    /* GPIO0-31, the next one is GPIO32-48 (+0x0C) */
#define ESP32S3_GPIO_OUT_W1TS_REG       (ESP32S3_GPIO_BASE + 0x08UL)
#define ESP32S3_GPIO_OUT_W1TC_REG       (ESP32S3_GPIO_BASE + 0x0CUL)
#define ESP32S3_GPIO_OUT1_REG           (ESP32S3_GPIO_BASE + 0x10UL)
#define ESP32S3_GPIO_OUT1_W1TS_REG      (ESP32S3_GPIO_BASE + 0x14UL)
#define ESP32S3_GPIO_OUT1_W1TC_REG      (ESP32S3_GPIO_BASE + 0x18UL)
#define ESP32S3_GPIO_ENABLE_REG         (ESP32S3_GPIO_BASE + 0x20UL)
#define ESP32S3_GPIO_ENABLE_W1TS_REG    (ESP32S3_GPIO_BASE + 0x24UL)
#define ESP32S3_GPIO_ENABLE_W1TC_REG    (ESP32S3_GPIO_BASE + 0x28UL)
#define ESP32S3_GPIO_ENABLE1_REG        (ESP32S3_GPIO_BASE + 0x2CUL)
#define ESP32S3_GPIO_ENABLE1_W1TS_REG   (ESP32S3_GPIO_BASE + 0x30UL)
#define ESP32S3_GPIO_ENABLE1_W1TC_REG   (ESP32S3_GPIO_BASE + 0x34UL)
#define ESP32S3_GPIO_IN_REG             (ESP32S3_GPIO_BASE + 0x3CUL)
#define ESP32S3_GPIO_IN1_REG            (ESP32S3_GPIO_BASE + 0x40UL)
#define ESP32S3_GPIO_PIN_REG(n)         (ESP32S3_GPIO_BASE + 0x74UL + 4UL * (n))
#define ESP32S3_GPIO_FUNC_OUT_SEL_REG(n) (ESP32S3_GPIO_BASE + 0x554UL + 4UL * (n))
#define ESP32S3_GPIO_STATUS_REG         (ESP32S3_GPIO_BASE + 0x44UL)    /* the interrupts of GPIO0-31, the next one is GPIO32-48 (+0x0C) */
#define ESP32S3_GPIO_STATUS_W1TC_REG    (ESP32S3_GPIO_BASE + 0x4CUL)
#define ESP32S3_GPIO_STATUS1_REG        (ESP32S3_GPIO_BASE + 0x50UL)
#define ESP32S3_GPIO_STATUS1_W1TC_REG   (ESP32S3_GPIO_BASE + 0x58UL)
/** GPIO_PINn_REG: the interrupt type (0: off, 1: rising edge, 2: falling edge, 3: both edges, 4: low level, 5: high level) */
#define ESP32S3_GPIO_PIN_INT_TYPE_S     7U
#define ESP32S3_GPIO_PIN_INT_TYPE_M     (7UL << ESP32S3_GPIO_PIN_INT_TYPE_S)
/** GPIO_PINn_REG: the interrupt of the pad goes to the CPU (bit 13 of INT_ENA; the next ones are the NMI and the second core) */
#define ESP32S3_GPIO_PIN_INT_ENA_CPU    (1UL << 13)
/** GPIO_PINn_REG: the output stage is open-drain */
#define ESP32S3_GPIO_PIN_PAD_DRIVER     (1UL << 2)
/** GPIO_FUNCn_OUT_SEL_CFG_REG: the signal that drives the pad - the output register of the GPIO matrix */
#define ESP32S3_GPIO_SIG_GPIO_OUT       256UL

/* IO MUX (DR_REG_IO_MUX_BASE): one register per pad, GPIOn at +0x04 + 4 * n */
#define ESP32S3_IO_MUX_BASE             0x60009000UL
#define ESP32S3_IO_MUX_REG(n)           (ESP32S3_IO_MUX_BASE + 0x04UL + 4UL * (n))
#define ESP32S3_IO_MUX_FUN_PD           (1UL << 7)
#define ESP32S3_IO_MUX_FUN_PU           (1UL << 8)
#define ESP32S3_IO_MUX_FUN_IE           (1UL << 9)
#define ESP32S3_IO_MUX_FUN_DRV_S        10U
#define ESP32S3_IO_MUX_FUN_DRV_M        (3UL << ESP32S3_IO_MUX_FUN_DRV_S)
#define ESP32S3_IO_MUX_MCU_SEL_S        12U
#define ESP32S3_IO_MUX_MCU_SEL_M        (7UL << ESP32S3_IO_MUX_MCU_SEL_S)
/** The function of the pad that makes it a GPIO driven by the GPIO matrix */
#define ESP32S3_IO_MUX_FUNC_GPIO        1UL
/** The highest function of the IO MUX */
#define ESP32S3_IO_MUX_FUNC_MAX         5U

/*
 * The interrupt of the GPIO is one source of the interrupt matrix (ETS_GPIO_INTR_SOURCE = 16, level type) for all the pads. It
 * goes to this interrupt of the CPU: a level 1 one (the lines of level 1 may call the RTOS from their handlers), not used by the
 * firmware, the tick or the software interrupts. The firmware hands it to dmod (Dmod_IrqAll), and the handler of this module
 * (DMOD_IRQ_HANDLER) takes it from there.
 */
#define ESP32S3_GPIO_INTR_SOURCE        16U
#define ESP32S3_GPIO_CPU_INTR           2U
/** INTERRUPT_CORE0_GPIO_INTERRUPT_PRO_MAP_REG: the line of the CPU that the source of the GPIO goes to */
#define ESP32S3_INTMTX_GPIO_MAP_REG     (0x600C2000UL + 0x040UL)

/** The handler of the interrupt of the GPIO (the module registers it in port.c as the handler of ESP32S3_GPIO_CPU_INTR) */
void esp32s3_gpio_irq_handler(void);
/** Turns off the interrupts of all the pins and the line of the CPU, forgets the handlers (the unload of the module) */
void esp32s3_gpio_irq_shutdown(void);

#endif /* ESP32S3_GPIO_H */
