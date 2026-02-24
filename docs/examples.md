# DMGPIO Usage Examples

## Example 1: Blinking an LED

**config.ini:**
```ini
[dmgpio]
port=A
pin=5
mode=output_pp
pull=none
speed=low
alternate=0
```

**Code:**
```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"

void blink_example(void)
{
    dmini_context_t config = dmini_load("config.ini");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t gpio_ctx = dmgpio_dmdrvi_create(config, &dev_num);
    void* handle = dmgpio_dmdrvi_open(gpio_ctx, DMDRVI_O_RDWR);

    for (int i = 0; i < 10; i++)
    {
        dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_toggle, NULL);
        // delay here
    }

    dmgpio_dmdrvi_close(gpio_ctx, handle);
    dmgpio_dmdrvi_free(gpio_ctx);
    dmini_free(config);
}
```

---

## Example 2: Reading a Button

**config.ini:**
```ini
[dmgpio]
port=C
pin=13
mode=input
pull=down
speed=low
alternate=0
```

**Code:**
```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"

void button_example(void)
{
    dmini_context_t config = dmini_load("button.ini");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t gpio_ctx = dmgpio_dmdrvi_create(config, &dev_num);
    void* handle = dmgpio_dmdrvi_open(gpio_ctx, DMDRVI_O_RDONLY);

    dmgpio_pin_state_t state;
    dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_get_state, &state);

    if (state == dmgpio_pin_set)
    {
        // Button is pressed
    }

    dmgpio_dmdrvi_close(gpio_ctx, handle);
    dmgpio_dmdrvi_free(gpio_ctx);
    dmini_free(config);
}
```

---

## Example 3: Reconfiguring a Pin at Runtime

```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"

void reconfigure_example(dmdrvi_context_t gpio_ctx, void* handle)
{
    // Switch from output to input
    dmgpio_mode_t new_mode = dmgpio_mode_input;
    dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_set_mode, &new_mode);

    // Enable pull-up
    dmgpio_pull_t new_pull = dmgpio_pull_up;
    dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_set_pull, &new_pull);

    // Apply changes to hardware
    dmgpio_dmdrvi_ioctl(gpio_ctx, handle, dmgpio_ioctl_cmd_reconfigure, NULL);
}
```

---

## Example 4: Using the Read Interface

```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

void read_example(dmdrvi_context_t gpio_ctx, void* handle)
{
    char buf[128];
    size_t n = dmgpio_dmdrvi_read(gpio_ctx, handle, buf, sizeof(buf));
    if (n > 0)
    {
        printf("GPIO state: %.*s\n", (int)n, buf);
        // Output: port=A;pin=5;state=1;mode=output_pp;pull=none;speed=low
    }
}
```

---

## Example 5: Using Write Interface

```c
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"

void write_example(dmdrvi_context_t gpio_ctx, void* handle)
{
    // Set pin high by writing "1"
    dmgpio_dmdrvi_write(gpio_ctx, handle, "1", 1);

    // Set pin low by writing "0"
    dmgpio_dmdrvi_write(gpio_ctx, handle, "0", 1);
}
```
