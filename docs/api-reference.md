# DMGPIO API Reference

## Types

### `dmgpio_mode_t`

GPIO pin operating mode.

```c
typedef enum
{
    dmgpio_mode_input = 0,      // Digital input
    dmgpio_mode_output_pp,      // Output push-pull
    dmgpio_mode_output_od,      // Output open-drain
    dmgpio_mode_af_pp,          // Alternate function push-pull
    dmgpio_mode_af_od,          // Alternate function open-drain
    dmgpio_mode_analog,         // Analog mode (ADC/DAC)
} dmgpio_mode_t;
```

### `dmgpio_pull_t`

Internal pull resistor configuration.

```c
typedef enum
{
    dmgpio_pull_none = 0,       // No pull resistor
    dmgpio_pull_up,             // Pull-up resistor
    dmgpio_pull_down,           // Pull-down resistor
} dmgpio_pull_t;
```

### `dmgpio_speed_t`

GPIO output slew rate.

```c
typedef enum
{
    dmgpio_speed_low = 0,       // Low speed (~2 MHz)
    dmgpio_speed_medium,        // Medium speed (~25 MHz)
    dmgpio_speed_high,          // High speed (~50 MHz)
    dmgpio_speed_very_high,     // Very high speed (~100 MHz)
} dmgpio_speed_t;
```

### `dmgpio_pin_state_t`

GPIO pin logic level.

```c
typedef enum
{
    dmgpio_pin_reset = 0,       // Logic low (0)
    dmgpio_pin_set,             // Logic high (1)
} dmgpio_pin_state_t;
```

### `dmgpio_ioctl_cmd_t`

IOCTL command codes for `dmgpio_dmdrvi_ioctl`.

```c
typedef enum
{
    dmgpio_ioctl_cmd_get_state = 1, // Read current pin state -> dmgpio_pin_state_t*
    dmgpio_ioctl_cmd_set_state,     // Write pin state <- dmgpio_pin_state_t*
    dmgpio_ioctl_cmd_toggle,        // Toggle pin (arg = NULL)
    dmgpio_ioctl_cmd_get_mode,      // Read pin mode -> dmgpio_mode_t*
    dmgpio_ioctl_cmd_set_mode,      // Write pin mode <- dmgpio_mode_t*
    dmgpio_ioctl_cmd_get_pull,      // Read pull config -> dmgpio_pull_t*
    dmgpio_ioctl_cmd_set_pull,      // Write pull config <- dmgpio_pull_t*
    dmgpio_ioctl_cmd_get_speed,     // Read output speed -> dmgpio_speed_t*
    dmgpio_ioctl_cmd_set_speed,     // Write output speed <- dmgpio_speed_t*
    dmgpio_ioctl_cmd_reconfigure,   // Reapply current configuration (arg = NULL)
    dmgpio_ioctl_cmd_max
} dmgpio_ioctl_cmd_t;
```

## DMDRVI Functions

### `dmgpio_dmdrvi_create`

Create a new GPIO device context by reading configuration from an INI file.

```c
dmdrvi_context_t dmgpio_dmdrvi_create(dmini_context_t config, dmdrvi_dev_num_t* dev_num);
```

**Parameters:**
- `config` – INI configuration context (see [Configuration Guide](configuration.md))
- `dev_num` – Device number output

**Returns:** New context on success, `NULL` on failure.

---

### `dmgpio_dmdrvi_free`

Free the GPIO device context and deinitialize the pin.

```c
void dmgpio_dmdrvi_free(dmdrvi_context_t context);
```

---

### `dmgpio_dmdrvi_open`

Open a handle to the GPIO device.

```c
void* dmgpio_dmdrvi_open(dmdrvi_context_t context, int flags);
```

**Parameters:**
- `context` – DMDRVI context
- `flags` – Open flags (e.g., `DMDRVI_O_RDWR`)

**Returns:** Device handle on success, `NULL` on failure.

---

### `dmgpio_dmdrvi_close`

Close the device handle.

```c
void dmgpio_dmdrvi_close(dmdrvi_context_t context, void* handle);
```

---

### `dmgpio_dmdrvi_read`

Read current device state as a formatted string.

```c
size_t dmgpio_dmdrvi_read(dmdrvi_context_t context, void* handle, void* buffer, size_t size);
```

**Output format:**
```
port=<A-K>;pin=<0-15>;state=<0|1>;mode=<mode>;pull=<pull>;speed=<speed>
```

**Returns:** Number of bytes written to `buffer`.

---

### `dmgpio_dmdrvi_write`

Write pin state by passing a string to the device.

```c
size_t dmgpio_dmdrvi_write(dmdrvi_context_t context, void* handle, const void* buffer, size_t size);
```

Writing `"0"` resets the pin; any other value sets it.

---

### `dmgpio_dmdrvi_ioctl`

Perform control and query operations on the GPIO device.

```c
int dmgpio_dmdrvi_ioctl(dmdrvi_context_t context, void* handle, int command, void* arg);
```

**Parameters:**
- `command` – IOCTL command from `dmgpio_ioctl_cmd_t`
- `arg` – Command-specific argument (see command descriptions above)

**Returns:** 0 on success, negative errno on failure.

---

### `dmgpio_dmdrvi_stat`

Get device statistics.

```c
int dmgpio_dmdrvi_stat(dmdrvi_context_t context, void* handle, dmdrvi_stat_t* stat);
```

## Port API

The port layer defines hardware-specific functions. See `include/dmgpio_port.h`.

### `dmgpio_port_init`

Initialize a GPIO pin.

```c
int dmgpio_port_init(dmgpio_port_t port, dmgpio_pin_t pin,
                     dmgpio_mode_t mode, dmgpio_pull_t pull,
                     dmgpio_speed_t speed, uint8_t alternate);
```

### `dmgpio_port_deinit`

Deinitialize (reset) a GPIO pin.

```c
int dmgpio_port_deinit(dmgpio_port_t port, dmgpio_pin_t pin);
```

### `dmgpio_port_read_pin`

Read the current logical state of a GPIO pin.

```c
dmgpio_pin_state_t dmgpio_port_read_pin(dmgpio_port_t port, dmgpio_pin_t pin);
```

### `dmgpio_port_write_pin`

Set the logical state of a GPIO output pin.

```c
void dmgpio_port_write_pin(dmgpio_port_t port, dmgpio_pin_t pin, dmgpio_pin_state_t state);
```

### `dmgpio_port_toggle_pin`

Toggle the logical state of a GPIO output pin.

```c
void dmgpio_port_toggle_pin(dmgpio_port_t port, dmgpio_pin_t pin);
```
