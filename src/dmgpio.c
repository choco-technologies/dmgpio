#define DMOD_ENABLE_REGISTRATION    ON
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmgpio_port.h"
#include <errno.h>
#include <string.h>

/* Magic set to DGPIO */
#define DMGPIO_CONTEXT_MAGIC    0x44475049

/* Reference counter for driver turn-on/turn-off */
static uint32_t driver_ref_count;

/**
 * @brief DMDRVI context structure
 */
struct dmdrvi_context
{
    uint32_t         magic;     /**< Magic number for validation */
    dmgpio_config_t  config;    /**< GPIO configuration */
};

static int is_valid_context(dmdrvi_context_t context)
{
    return (context != NULL && context->magic == DMGPIO_CONTEXT_MAGIC);
}

/* ---- String helpers ---- */

static const char *mode_to_string(dmgpio_mode_t mode)
{
    switch (mode)
    {
        case dmgpio_mode_input:     return "input";
        case dmgpio_mode_output:    return "output";
        case dmgpio_mode_alternate: return "alternate";
        default:                    return "input";
    }
}

static dmgpio_mode_t string_to_mode(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "output")    == 0) return dmgpio_mode_output;
        if (strcmp(s, "alternate") == 0) return dmgpio_mode_alternate;
    }
    return dmgpio_mode_input;
}

static const char *output_circuit_to_string(dmgpio_output_circuit_t oc)
{
    switch (oc)
    {
        case dmgpio_output_circuit_open_drain: return "open_drain";
        case dmgpio_output_circuit_push_pull:  return "push_pull";
        default:                               return "push_pull";
    }
}

static dmgpio_output_circuit_t string_to_output_circuit(const char *s)
{
    if (s != NULL && strcmp(s, "open_drain") == 0) return dmgpio_output_circuit_open_drain;
    return dmgpio_output_circuit_push_pull;
}

static const char *pull_to_string(dmgpio_pull_t pull)
{
    switch (pull)
    {
        case dmgpio_pull_up:   return "up";
        case dmgpio_pull_down: return "down";
        default:               return "none";
    }
}

static dmgpio_pull_t string_to_pull(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "up")   == 0) return dmgpio_pull_up;
        if (strcmp(s, "down") == 0) return dmgpio_pull_down;
    }
    return dmgpio_pull_default;
}

static const char *speed_to_string(dmgpio_speed_t speed)
{
    switch (speed)
    {
        case dmgpio_speed_minimum: return "minimum";
        case dmgpio_speed_medium:  return "medium";
        case dmgpio_speed_maximum: return "maximum";
        default:                   return "minimum";
    }
}

static dmgpio_speed_t string_to_speed(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "medium")  == 0) return dmgpio_speed_medium;
        if (strcmp(s, "maximum") == 0) return dmgpio_speed_maximum;
    }
    return dmgpio_speed_minimum;
}

static dmgpio_port_t string_to_port(const char *s)
{
    if (s != NULL && s[0] >= 'A' && s[0] <= 'K')
        return (dmgpio_port_t)(s[0] - 'A');
    return 0;
}

static const char *port_to_string(dmgpio_port_t port)
{
    static const char *names[] = { "A","B","C","D","E","F","G","H","I","J","K" };
    if (port < (sizeof(names) / sizeof(names[0])))
        return names[port];
    return "A";
}

/* ---- Configuration helpers ---- */

static int check_config(const dmgpio_config_t *cfg)
{
    if (cfg->pin > 15)
    {
        DMOD_LOG_ERROR("Invalid pin number %d, must be 0-15\n", cfg->pin);
        return -EINVAL;
    }
    return 0;
}

static int read_config_parameters(dmdrvi_context_t ctx, dmini_context_t ini)
{
    ctx->config.port            = string_to_port(dmini_get_string(ini, "dmgpio", "port", "A"));
    ctx->config.pin             = (dmgpio_pin_t)dmini_get_int(ini, "dmgpio", "pin", 0);
    ctx->config.mode            = string_to_mode(dmini_get_string(ini, "dmgpio", "mode", "input"));
    ctx->config.pull            = string_to_pull(dmini_get_string(ini, "dmgpio", "pull", "none"));
    ctx->config.speed           = string_to_speed(dmini_get_string(ini, "dmgpio", "speed", "minimum"));
    ctx->config.output_circuit  = string_to_output_circuit(dmini_get_string(ini, "dmgpio", "output_circuit", "push_pull"));
    ctx->config.current         = dmgpio_current_default;
    ctx->config.protection      = dmgpio_protection_dont_unlock_protected_pins;
    ctx->config.interrupt_trigger = dmgpio_int_trigger_off;
    return check_config(&ctx->config);
}

static int configure(dmdrvi_context_t ctx)
{
    const dmgpio_config_t *c = &ctx->config;
    dmgpio_pins_mask_t pin_mask = (dmgpio_pins_mask_t)(1U << c->pin);

    dmgpio_port_set_power(c->port, 1);

    dmgpio_port_begin_configuration(c->port, c->pin);
    dmgpio_port_set_mode(c->port, c->pin, c->mode);
    dmgpio_port_set_pull(c->port, c->pin, c->pull);
    dmgpio_port_set_speed(c->port, c->pin, c->speed);
    dmgpio_port_set_output_circuit(c->port, c->pin, c->output_circuit);
    dmgpio_port_set_current(c->port, c->pin, c->current);
    if (c->interrupt_trigger != dmgpio_int_trigger_off)
        dmgpio_port_set_interrupt_trigger(c->port, c->pin, c->interrupt_trigger);
    dmgpio_port_finish_configuration(c->port, c->pin);

    dmgpio_port_set_pins_used(c->port, pin_mask);

    DMOD_LOG_INFO("GPIO P%s%d configured: mode=%s, pull=%s, speed=%s, circuit=%s\n",
        port_to_string(c->port), c->pin,
        mode_to_string(c->mode),
        pull_to_string(c->pull),
        speed_to_string(c->speed),
        output_circuit_to_string(c->output_circuit));
    return 0;
}

/* ---- DMDRVI interface ---- */

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, dmdrvi_context_t, _create,
    ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    dmdrvi_context_t ctx = (dmdrvi_context_t)Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (ctx == NULL)
    {
        DMOD_LOG_ERROR("Failed to allocate DMDRVI context\n");
        return NULL;
    }

    Dmod_Memset(ctx, 0, sizeof(struct dmdrvi_context));
    ctx->magic = DMGPIO_CONTEXT_MAGIC;

    if (read_config_parameters(ctx, config) != 0)
    {
        DMOD_LOG_ERROR("Failed to read GPIO configuration\n");
        Dmod_Free(ctx);
        return NULL;
    }

    if (driver_ref_count == 0)
        dmgpio_port_turn_on_driver();
    driver_ref_count++;

    if (configure(ctx) != 0)
    {
        DMOD_LOG_ERROR("Failed to configure GPIO pin\n");
        driver_ref_count--;
        if (driver_ref_count == 0)
            dmgpio_port_turn_off_driver();
        Dmod_Free(ctx);
        return NULL;
    }

    DMOD_LOG_INFO("GPIO device created for P%s%d\n",
        port_to_string(ctx->config.port), ctx->config.pin);
    return ctx;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, void, _free, ( dmdrvi_context_t context ))
{
    if (is_valid_context(context))
    {
        dmgpio_pins_mask_t pin_mask = (dmgpio_pins_mask_t)(1U << context->config.pin);
        dmgpio_port_set_pins_unused(context->config.port, pin_mask);
        context->magic = 0;
        Dmod_Free(context);
        if (driver_ref_count > 0)
        {
            driver_ref_count--;
            if (driver_ref_count == 0)
                dmgpio_port_turn_off_driver();
        }
    }
}

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, void*, _open,
    ( dmdrvi_context_t context, int flags ))
{
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmgpio_dmdrvi_open\n");
        return NULL;
    }
    return context;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, void, _close,
    ( dmdrvi_context_t context, void* handle ))
{
    /* No action needed */
}

/**
 * @brief Read: returns bitmask of pins currently in high state
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, size_t, _read,
    ( dmdrvi_context_t context, void* handle, void* buffer, size_t size ))
{
    if (!is_valid_context(context))
        return 0;

    dmgpio_pins_mask_t pin_mask = (dmgpio_pins_mask_t)(1U << context->config.pin);
    dmgpio_pins_mask_t high_pins = dmgpio_port_get_high_state_pins(context->config.port, pin_mask);

    int written = Dmod_SnPrintf(buffer, size,
        "port=%s;pin=%d;high_pins=0x%04X",
        port_to_string(context->config.port),
        context->config.pin,
        (unsigned)high_pins);
    return (written > 0) ? (size_t)written : 0;
}

/**
 * @brief Write: buffer[0]='0' → low, otherwise → high
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, size_t, _write,
    ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size ))
{
    if (!is_valid_context(context) || buffer == NULL || size == 0)
        return 0;

    const char *data = (const char *)buffer;
    dmgpio_pins_mask_t pin_mask = (dmgpio_pins_mask_t)(1U << context->config.pin);
    dmgpio_pins_state_t state = (data[0] == '0') ?
        dmgpio_pins_state_all_low : dmgpio_pins_state_all_high;
    dmgpio_port_set_pins_state(context->config.port, pin_mask, state);
    return size;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, int, _ioctl,
    ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmgpio_dmdrvi_ioctl\n");
        return -EINVAL;
    }

    dmgpio_pins_mask_t pin_mask = (dmgpio_pins_mask_t)(1U << context->config.pin);

    switch ((dmgpio_ioctl_cmd_t)command)
    {
        case dmgpio_ioctl_cmd_toggle_pins:
            dmgpio_port_toggle_pins_state(context->config.port, pin_mask);
            return 0;

        case dmgpio_ioctl_cmd_set_pins_state:
            if (arg == NULL) return -EINVAL;
            dmgpio_port_set_pins_state(context->config.port, pin_mask,
                *(dmgpio_pins_state_t *)arg);
            return 0;

        case dmgpio_ioctl_cmd_get_high_pins_state:
            if (arg == NULL) return -EINVAL;
            *(dmgpio_pins_mask_t *)arg =
                dmgpio_port_get_high_state_pins(context->config.port, pin_mask);
            return 0;

        case dmgpio_ioctl_cmd_get_low_pins_state:
            if (arg == NULL) return -EINVAL;
            *(dmgpio_pins_mask_t *)arg =
                dmgpio_port_get_low_state_pins(context->config.port, pin_mask);
            return 0;

        default:
            DMOD_LOG_ERROR("Unknown ioctl command %d\n", command);
            return -EINVAL;
    }
}

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, int, _flush,
    ( dmdrvi_context_t context, void* handle ))
{
    return 0;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, int, _stat,
    ( dmdrvi_context_t context, void* handle, dmdrvi_stat_t* stat ))
{
    if (!is_valid_context(context) || stat == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters in dmgpio_dmdrvi_stat\n");
        return -EINVAL;
    }
    stat->size = 1;
    stat->mode = 0666;
    return 0;
}
