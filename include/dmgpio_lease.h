#ifndef DMGPIO_LEASE_H
#define DMGPIO_LEASE_H
#include "dmgpio.h"
typedef struct dmgpio_lease *dmgpio_lease_t;
typedef void (*dmgpio_edge_t)(void *user, bool high);
/* pin = port*16+index. All lifecycle calls are task-only; read/write are ISR-safe.
 * Callbacks run in GPIO IRQ context; release disables them before freeing state. */
dmod_dmgpio_api(1.0, int, _pin_acquire, (int16_t pin, dmgpio_mode_t mode, uint8_t af, bool initial_high, dmgpio_edge_t edge, void *user, dmgpio_lease_t *out));
dmod_dmgpio_api(1.0, void, _pin_release, (dmgpio_lease_t pin));
dmod_dmgpio_api(1.0, void, _pin_write, (dmgpio_lease_t pin, bool high));
dmod_dmgpio_api(1.0, bool, _pin_read, (dmgpio_lease_t pin));
#endif
