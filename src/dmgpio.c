#define DMOD_ENABLE_REGISTRATION    ON
#include "dmod.h"
#include "dmgpio.h"
#include "dmgpio_port.h"
#include "dmdrvi.h"
#include <errno.h>
#include <string.h>

/* Magic set to DGPIO */
#define DMGPIO_CONTEXT_MAGIC    0x44475049

/**
 * @brief Maximum number of interrupt handlers that can be registered on one context.
 *
 * Additional handlers beyond this limit are rejected with -ENOMEM.
 * Adjust this compile-time constant if more handlers per context are needed.
 */
#define DMGPIO_MAX_HANDLERS_PER_CONTEXT  4U

/**
 * @brief DMDRVI context structure
 */
struct dmdrvi_context
{
    uint32_t                    magic;                                          /**< Magic number for validation */
    dmgpio_config_t             config;                                         /**< GPIO configuration */
    dmgpio_interrupt_handler_t  handlers[DMGPIO_MAX_HANDLERS_PER_CONTEXT];     /**< Registered user handlers */
    size_t                      handler_count;                                  /**< Number of registered handlers */
};

static int is_valid_context(dmdrvi_context_t context)
{
    return (context != NULL && context->magic == DMGPIO_CONTEXT_MAGIC);
}

/* ---- Per-context interrupt handler ---- */

/**
 * @brief Port-level interrupt handler registered per context in dmgpio_port.
 *
 * Called by the port layer with @p user_ptr set to the owning context.
 * Filters the interrupt to pins owned by this context and calls all
 * registered user handlers.
 */
static void dmgpio_ctx_irq_handler(void *user_ptr, dmgpio_port_t port, dmgpio_pins_mask_t pins)
{
    dmdrvi_context_t ctx = (dmdrvi_context_t)user_ptr;
    dmgpio_pins_mask_t matching = (dmgpio_pins_mask_t)(ctx->config.pins & pins);
    if (matching == 0U) return;
    for (size_t i = 0; i < ctx->handler_count; i++)
        ctx->handlers[i](ctx, port, matching);
}

/* ---- String helpers ---- */

static const char *mode_to_string(dmgpio_mode_t mode)
{
    switch (mode)
    {
        case dmgpio_mode_input:     return "input";
        case dmgpio_mode_output:    return "output";
        case dmgpio_mode_alternate: return "alternate";
        default:                    return "unknown";
    }
}

static int string_to_mode(const char *s, dmgpio_mode_t *out_mode)
{
    if (s != NULL)
    {
        if (strcmp(s, "input")     == 0) { *out_mode = dmgpio_mode_input;     return 0; }
        if (strcmp(s, "output")    == 0) { *out_mode = dmgpio_mode_output;    return 0; }
        if (strcmp(s, "alternate") == 0) { *out_mode = dmgpio_mode_alternate; return 0; }
    }
    return -1;
}

static const char *output_circuit_to_string(dmgpio_output_circuit_t oc)
{
    switch (oc)
    {
        case dmgpio_output_circuit_open_drain: return "open_drain";
        case dmgpio_output_circuit_push_pull:  return "push_pull";
        default:                               return "default";
    }
}

static dmgpio_output_circuit_t string_to_output_circuit(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "open_drain") == 0) return dmgpio_output_circuit_open_drain;
        if (strcmp(s, "push_pull")  == 0) return dmgpio_output_circuit_push_pull;
    }
    return dmgpio_output_circuit_default;
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
        default:                   return "default";
    }
}

static dmgpio_speed_t string_to_speed(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "minimum") == 0) return dmgpio_speed_minimum;
        if (strcmp(s, "medium")  == 0) return dmgpio_speed_medium;
        if (strcmp(s, "maximum") == 0) return dmgpio_speed_maximum;
    }
    return dmgpio_speed_default;
}

static dmgpio_current_t string_to_current(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "minimum") == 0) return dmgpio_current_minimum;
        if (strcmp(s, "medium")  == 0) return dmgpio_current_medium;
        if (strcmp(s, "maximum") == 0) return dmgpio_current_maximum;
    }
    return dmgpio_current_default;
}

static dmgpio_protection_t string_to_protection(const char *s)
{
    if (s != NULL && strcmp(s, "unlock") == 0)
        return dmgpio_protection_unlock_protected_pins;
    return dmgpio_protection_dont_unlock_protected_pins;
}

static dmgpio_int_trigger_t string_to_interrupt_trigger(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "rising_edge")  == 0) return dmgpio_int_trigger_rising_edge;
        if (strcmp(s, "falling_edge") == 0) return dmgpio_int_trigger_falling_edge;
        if (strcmp(s, "both_edges")   == 0) return dmgpio_int_trigger_both_edges;
        if (strcmp(s, "high_level")   == 0) return dmgpio_int_trigger_high_level;
        if (strcmp(s, "low_level")    == 0) return dmgpio_int_trigger_low_level;
        if (strcmp(s, "both_levels")  == 0) return dmgpio_int_trigger_both_levels;
    }
    return dmgpio_int_trigger_off;
}

static int string_to_port(const char *s, dmgpio_port_t *out_port)
{
    if (s != NULL && s[0] >= 'A' && s[0] <= 'K' && s[1] == '\0')
    {
        *out_port = (dmgpio_port_t)(s[0] - 'A');
        return 0;
    }
    return -1;
}

static const char *port_to_string(dmgpio_port_t port)
{
    static const char *names[] = { "A","B","C","D","E","F","G","H","I","J","K" };
    if (port < (sizeof(names) / sizeof(names[0]))) return names[port];
    return "?";
}

/* ---- Configuration helpers ---- */

/**
 * @brief Parse a decimal or hex (0x-prefixed) unsigned integer string.
 *
 * @param s       Null-terminated input string (must not be NULL).
 * @param out_val Receives the parsed value on success.
 * @return 0 on success, -1 on parse error or overflow (value > ULONG_MAX/UINT16_MAX).
 *
 * Note: overflow is detected conservatively — parsing stops with an error if the
 * accumulated value would exceed 0xFFFF, which is sufficient for pin-mask values.
 */
static int parse_uint(const char *s, unsigned long *out_val)
{
    if (s == NULL || *s == '\0') return -1;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        /* Hex */
        const char *p = s + 2;
        if (*p == '\0') return -1;
        unsigned long v = 0;
        for (; *p != '\0'; p++)
        {
            unsigned char c = (unsigned char)*p;
            unsigned long digit;
            if (c >= '0' && c <= '9')      digit = (unsigned long)(c - '0');
            else if (c >= 'a' && c <= 'f') digit = (unsigned long)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = (unsigned long)(c - 'A' + 10);
            else return -1;
            if (v > (0xFFFFUL >> 4U)) return -1; /* overflow guard */
            v = (v << 4U) | digit;
        }
        *out_val = v;
        return 0;
    }

    /* Decimal */
    const char *p = s;
    if (*p == '\0') return -1;
    unsigned long v = 0;
    for (; *p != '\0'; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (c < '0' || c > '9') return -1;
        if (v > (0xFFFFUL / 10U)) return -1; /* overflow guard */
        v = v * 10U + (unsigned long)(c - '0');
        if (v > 0xFFFFUL) return -1;
    }
    *out_val = v;
    return 0;
}

/**
 * @brief Parse the section name to resolve port and pins configuration.
 *
 * Supports two formats:
 *   1. Combined: pin=PA5   (sets port=A, pins=1<<5)
 *   2. Separate: port=A / pins=0x0020  (decimal or hex bitmask)
 */
static int read_port_and_pins(dmini_context_t ini, const char *section,
                               dmgpio_port_t *out_port, dmgpio_pins_mask_t *out_pins)
{
    /* Try combined "pin=PA5" or "pin=A5" format first */
    const char *pin_str = dmini_get_string(ini, section, "pin", NULL);
    if (pin_str != NULL)
    {
        /* Accept optional leading 'P' (e.g. "PA5" or "A5") */
        const char *port_ptr = pin_str;
        if (port_ptr[0] == 'P') port_ptr++;

        if (port_ptr[0] >= 'A' && port_ptr[0] <= 'K' && port_ptr[1] != '\0')
        {
            unsigned long pin_num;
            if (parse_uint(port_ptr + 1, &pin_num) != 0 || pin_num > 15)
            {
                DMOD_LOG_ERROR("Invalid pin in '%s' config 'pin=%s' (expected PA0-PK15 or A0-K15)\n",
                    section, pin_str);
                return -EINVAL;
            }
            *out_port = (dmgpio_port_t)(port_ptr[0] - 'A');
            *out_pins = (dmgpio_pins_mask_t)(1U << pin_num);
            return 0;
        }
    }

    /* Fall back to separate port= and pins= keys */
    const char *port_str = dmini_get_string(ini, section, "port", NULL);
    if (string_to_port(port_str, out_port) != 0)
    {
        DMOD_LOG_ERROR("Invalid or missing 'port' in [%s] config (expected A-K)\n", section);
        return -EINVAL;
    }

    const char *pins_str = dmini_get_string(ini, section, "pins", NULL);
    if (pins_str == NULL)
    {
        DMOD_LOG_ERROR("Missing 'pins' in [%s] config\n", section);
        return -EINVAL;
    }
    unsigned long pins_val;
    if (parse_uint(pins_str, &pins_val) != 0 || pins_val < 1 || pins_val > 0xFFFF)
    {
        DMOD_LOG_ERROR("Invalid 'pins' in [%s] config (must be 1-0xFFFF bitmask)\n", section);
        return -EINVAL;
    }
    *out_pins = (dmgpio_pins_mask_t)pins_val;
    return 0;
}

static int read_config_parameters(dmdrvi_context_t ctx, dmini_context_t ini)
{
    if (read_port_and_pins(ini, "dmgpio", &ctx->config.port, &ctx->config.pins) != 0)
        return -EINVAL;

    /* Mode is mandatory */
    const char *mode_str = dmini_get_string(ini, "dmgpio", "mode", NULL);
    if (string_to_mode(mode_str, &ctx->config.mode) != 0)
    {
        DMOD_LOG_ERROR("Invalid or missing 'mode' in [dmgpio] config (expected input/output/alternate)\n");
        return -EINVAL;
    }

    ctx->config.pull             = string_to_pull(dmini_get_string(ini, "dmgpio", "pull", "none"));
    ctx->config.speed            = string_to_speed(dmini_get_string(ini, "dmgpio", "speed", "default"));
    ctx->config.output_circuit   = string_to_output_circuit(dmini_get_string(ini, "dmgpio", "output_circuit", "default"));
    ctx->config.current          = string_to_current(dmini_get_string(ini, "dmgpio", "current", "default"));
    ctx->config.protection       = string_to_protection(dmini_get_string(ini, "dmgpio", "protection", "dont_unlock"));
    ctx->config.interrupt_trigger = string_to_interrupt_trigger(dmini_get_string(ini, "dmgpio", "interrupt_trigger", "off"));
    ctx->config.interrupt_handler = NULL; /* set programmatically or via ioctl */

    return 0;
}

static int configure(dmdrvi_context_t ctx)
{
    const dmgpio_config_t *c = &ctx->config;
    int ret;

    ret = dmgpio_port_set_power(c->port, 1);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to enable power for GPIO port %s\n", port_to_string(c->port));
        return ret;
    }

    ret = dmgpio_port_begin_configuration(c->port, c->pins);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to begin configuration for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    if (c->protection == dmgpio_protection_unlock_protected_pins)
    {
        ret = dmgpio_port_unlock_protection(c->port, c->pins, c->protection);
        if (ret != 0)
        {
            DMOD_LOG_ERROR("Failed to unlock protection for GPIO port %s pins 0x%04X\n",
                port_to_string(c->port), (unsigned)c->pins);
            return ret;
        }
    }

    ret = dmgpio_port_set_mode(c->port, c->pins, c->mode);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to set mode for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    ret = dmgpio_port_set_pull(c->port, c->pins, c->pull);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to set pull for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    ret = dmgpio_port_set_speed(c->port, c->pins, c->speed);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to set speed for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    ret = dmgpio_port_set_output_circuit(c->port, c->pins, c->output_circuit);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to set output circuit for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    ret = dmgpio_port_set_current(c->port, c->pins, c->current);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to set current for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    ret = dmgpio_port_set_interrupt_trigger(c->port, c->pins, c->interrupt_trigger);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to set interrupt trigger for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    ret = dmgpio_port_finish_configuration(c->port, c->pins);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to finish configuration for GPIO port %s pins 0x%04X\n",
            port_to_string(c->port), (unsigned)c->pins);
        return ret;
    }

    dmgpio_port_set_pins_used(c->port, c->pins);

    DMOD_LOG_INFO("GPIO P%s[0x%04X] configured: mode=%s, pull=%s, speed=%s, circuit=%s\n",
        port_to_string(c->port), (unsigned)c->pins,
        mode_to_string(c->mode),
        pull_to_string(c->pull),
        speed_to_string(c->speed),
        output_circuit_to_string(c->output_circuit));
    return 0;
}

/**
 * @brief Initialize the DMDRVI module
 * 
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMGPIO module initialized (STM32F7)\n");
    return 0;
}

/**
 * @brief Deinitialize the DMDRVI module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    Dmod_Printf("DMGPIO module deinitialized (STM32F7)\n");
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

    memset(ctx, 0, sizeof(struct dmdrvi_context));
    ctx->magic = DMGPIO_CONTEXT_MAGIC;

    if (read_config_parameters(ctx, config) != 0)
    {
        DMOD_LOG_ERROR("Failed to read GPIO configuration\n");
        Dmod_Free(ctx);
        return NULL;
    }

    /* Register this context as a per-port handler in dmgpio_port, passing ctx
     * as user_ptr so the handler receives the context directly without any
     * global list scan. */
    if (dmgpio_port_set_driver_interrupt_handler(ctx->config.port,
                                                  dmgpio_ctx_irq_handler, ctx) != 0)
    {
        DMOD_LOG_ERROR("Failed to register port interrupt handler\n");
        Dmod_Free(ctx);
        return NULL;
    }

    /* Register the initial per-context handler provided in the config (optional). */
    if (ctx->config.interrupt_handler != NULL)
    {
        ctx->handlers[ctx->handler_count++] = ctx->config.interrupt_handler;
    }

    if (configure(ctx) != 0)
    {
        DMOD_LOG_ERROR("Failed to configure GPIO\n");
        dmgpio_port_remove_driver_interrupt_handler(ctx->config.port, ctx);
        Dmod_Free(ctx);
        return NULL;
    }

    DMOD_LOG_INFO("GPIO device created for P%s[0x%04X]\n",
        port_to_string(ctx->config.port), (unsigned)ctx->config.pins);
    return ctx;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, void, _free, ( dmdrvi_context_t context ))
{
    if (is_valid_context(context))
    {
        dmgpio_port_remove_driver_interrupt_handler(context->config.port, context);
        dmgpio_port_set_pins_unused(context->config.port, context->config.pins);
        context->magic = 0;
        Dmod_Free(context);
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

    dmgpio_pins_mask_t high_pins = dmgpio_port_get_high_state_pins(
        context->config.port, context->config.pins);

    int written = Dmod_SnPrintf(buffer, size,
        "port=%s;pins=0x%04X;high_pins=0x%04X",
        port_to_string(context->config.port),
        (unsigned)context->config.pins,
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
    dmgpio_pins_state_t state = (data[0] == '0') ?
        dmgpio_pins_state_all_low : dmgpio_pins_state_all_high;
    dmgpio_port_set_pins_state(context->config.port, context->config.pins, state);
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

    switch ((dmgpio_ioctl_cmd_t)command)
    {
        case dmgpio_ioctl_cmd_toggle_pins:
            dmgpio_port_toggle_pins_state(context->config.port, context->config.pins);
            return 0;

        case dmgpio_ioctl_cmd_set_pins_state:
            if (arg == NULL) return -EINVAL;
            dmgpio_port_set_pins_state(context->config.port, context->config.pins,
                *(dmgpio_pins_state_t *)arg);
            return 0;

        case dmgpio_ioctl_cmd_get_high_pins_state:
            if (arg == NULL) return -EINVAL;
            *(dmgpio_pins_mask_t *)arg =
                dmgpio_port_get_high_state_pins(context->config.port, context->config.pins);
            return 0;

        case dmgpio_ioctl_cmd_get_low_pins_state:
            if (arg == NULL) return -EINVAL;
            *(dmgpio_pins_mask_t *)arg =
                dmgpio_port_get_low_state_pins(context->config.port, context->config.pins);
            return 0;

        case dmgpio_ioctl_cmd_set_interrupt_handler:
            if (arg == NULL) return -EINVAL;
            {
                if (context->handler_count >= DMGPIO_MAX_HANDLERS_PER_CONTEXT)
                {
                    DMOD_LOG_ERROR("Maximum number of interrupt handlers reached\n");
                    return -ENOMEM;
                }
                context->handlers[context->handler_count++] = *(dmgpio_interrupt_handler_t *)arg;
            }
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