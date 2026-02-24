# DMGPIO Documentation

Welcome to the DMGPIO module documentation.

## Contents

- **[dmgpio.md](dmgpio.md)** - Module overview and architecture
- **[api-reference.md](api-reference.md)** - Complete API documentation
- **[configuration.md](configuration.md)** - Configuration guide with examples
- **[port-implementation.md](port-implementation.md)** - Guide for adding hardware support
- **[examples.md](examples.md)** - Usage examples

## Quick Reference

```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"

// Load configuration and create device
dmini_context_t config = dmini_load("config.ini");
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t gpio_ctx = dmgpio_dmdrvi_create(config, &dev_num);

// Open device
void* handle = dmgpio_dmdrvi_open(gpio_ctx, DMDRVI_O_RDWR);

// Set pin high
dmgpio_pin_state_t state = dmgpio_pin_set;
dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_set_state, &state);

// Toggle pin
dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_toggle, NULL);

// Cleanup
dmgpio_dmdrvi_close(gpio_ctx, handle);
dmgpio_dmdrvi_free(gpio_ctx);
dmini_free(config);
```
