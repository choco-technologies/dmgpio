#define DMOD_ENABLE_REGISTRATION    ON
#include "dmgpio.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmgpio_port.h"
#include <errno.h>
#include <string.h>

// Magic set to DGPIO
#define DMGPIO_CONTEXT_MAGIC    0x44475049

/**
 * @brief Configuration structure
 */
struct config
{
    dmgpio_port_t port;
    dmgpio_pin_t pin;
    dmgpio_mode_t mode;
    dmgpio_pull_t pull;
    dmgpio_speed_t speed;
    uint8_t alternate;
};

/**
 * @brief DMDRVI context structure
 */
struct dmdrvi_context
{
    uint32_t magic;             /**< Magic number for validation */
    struct config config;       /**< Configuration parameters */
    dmgpio_pin_state_t state;   /**< Current pin state */
};

/**
 * @brief Validate DMDRVI context
 *
 * @param context DMDRVI context to validate
 *
 * @return int 1 if valid, 0 otherwise
 */
static int is_valid_context(dmdrvi_context_t context)
{
    return (context != NULL && context->magic == DMGPIO_CONTEXT_MAGIC);
}

/**
 * @brief Convert GPIO mode enum to string
 *
 * @param mode GPIO mode
 *
 * @return const char* String representation of GPIO mode
 */
static const char* mode_to_string(dmgpio_mode_t mode)
{
    switch (mode)
    {
        case dmgpio_mode_input:      return "input";
        case dmgpio_mode_output_pp:  return "output_pp";
        case dmgpio_mode_output_od:  return "output_od";
        case dmgpio_mode_af_pp:      return "af_pp";
        case dmgpio_mode_af_od:      return "af_od";
        case dmgpio_mode_analog:     return "analog";
        default:                     return "unknown";
    }
}

/**
 * @brief Convert string to GPIO mode enum
 *
 * @param mode_str String representation of GPIO mode
 *
 * @return dmgpio_mode_t GPIO mode enum
 */
static dmgpio_mode_t string_to_mode(const char* mode_str)
{
    if (mode_str != NULL)
    {
        if (strcmp(mode_str, "input") == 0)         return dmgpio_mode_input;
        if (strcmp(mode_str, "output_pp") == 0)     return dmgpio_mode_output_pp;
        if (strcmp(mode_str, "output_od") == 0)     return dmgpio_mode_output_od;
        if (strcmp(mode_str, "af_pp") == 0)         return dmgpio_mode_af_pp;
        if (strcmp(mode_str, "af_od") == 0)         return dmgpio_mode_af_od;
        if (strcmp(mode_str, "analog") == 0)        return dmgpio_mode_analog;
    }
    return dmgpio_mode_input;
}

/**
 * @brief Convert GPIO pull enum to string
 *
 * @param pull GPIO pull configuration
 *
 * @return const char* String representation of pull configuration
 */
static const char* pull_to_string(dmgpio_pull_t pull)
{
    switch (pull)
    {
        case dmgpio_pull_none:  return "none";
        case dmgpio_pull_up:    return "up";
        case dmgpio_pull_down:  return "down";
        default:                return "none";
    }
}

/**
 * @brief Convert string to GPIO pull enum
 *
 * @param pull_str String representation of pull configuration
 *
 * @return dmgpio_pull_t GPIO pull enum
 */
static dmgpio_pull_t string_to_pull(const char* pull_str)
{
    if (pull_str != NULL)
    {
        if (strcmp(pull_str, "up") == 0)    return dmgpio_pull_up;
        if (strcmp(pull_str, "down") == 0)  return dmgpio_pull_down;
    }
    return dmgpio_pull_none;
}

/**
 * @brief Convert GPIO speed enum to string
 *
 * @param speed GPIO output speed
 *
 * @return const char* String representation of speed
 */
static const char* speed_to_string(dmgpio_speed_t speed)
{
    switch (speed)
    {
        case dmgpio_speed_low:        return "low";
        case dmgpio_speed_medium:     return "medium";
        case dmgpio_speed_high:       return "high";
        case dmgpio_speed_very_high:  return "very_high";
        default:                      return "low";
    }
}

/**
 * @brief Convert string to GPIO speed enum
 *
 * @param speed_str String representation of GPIO speed
 *
 * @return dmgpio_speed_t GPIO speed enum
 */
static dmgpio_speed_t string_to_speed(const char* speed_str)
{
    if (speed_str != NULL)
    {
        if (strcmp(speed_str, "medium") == 0)     return dmgpio_speed_medium;
        if (strcmp(speed_str, "high") == 0)       return dmgpio_speed_high;
        if (strcmp(speed_str, "very_high") == 0)  return dmgpio_speed_very_high;
    }
    return dmgpio_speed_low;
}

/**
 * @brief Convert port letter string to port index
 *
 * @param port_str Port letter string (e.g., "A", "B", ...)
 *
 * @return dmgpio_port_t Port index (0=A, 1=B, ...)
 */
static dmgpio_port_t string_to_port(const char* port_str)
{
    if (port_str != NULL && port_str[0] >= 'A' && port_str[0] <= 'K')
    {
        return (dmgpio_port_t)(port_str[0] - 'A');
    }
    return 0;
}

/**
 * @brief Convert port index to letter string
 *
 * @param port Port index (0=A, 1=B, ...)
 *
 * @return const char* Port letter string
 */
static const char* port_to_string(dmgpio_port_t port)
{
    static const char* port_names[] = {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K"
    };
    if (port < (sizeof(port_names) / sizeof(port_names[0])))
    {
        return port_names[port];
    }
    return "A";
}

/**
 * @brief Check configuration parameters
 *
 * @param cfg Configuration to validate
 *
 * @return int 0 if valid, non-zero otherwise
 */
static int check_config_parameters(struct config* cfg)
{
    if (cfg->pin > 15)
    {
        DMOD_LOG_ERROR("Invalid pin number %d, must be 0-15\n", cfg->pin);
        return -EINVAL;
    }
    return 0;
}

/**
 * @brief Read configuration parameters from Dmini context
 *
 * @param context DMDRVI context
 * @param config Dmini context with configuration data
 *
 * @return int 0 on success, non-zero on failure
 */
static int read_config_parameters(dmdrvi_context_t context, dmini_context_t config)
{
    context->config.port = string_to_port(dmini_get_string(config, "dmgpio", "port", "A"));
    context->config.pin = (dmgpio_pin_t)dmini_get_int(config, "dmgpio", "pin", 0);
    context->config.mode = string_to_mode(dmini_get_string(config, "dmgpio", "mode", "input"));
    context->config.pull = string_to_pull(dmini_get_string(config, "dmgpio", "pull", "none"));
    context->config.speed = string_to_speed(dmini_get_string(config, "dmgpio", "speed", "low"));
    context->config.alternate = (uint8_t)dmini_get_int(config, "dmgpio", "alternate", 0);

    return check_config_parameters(&context->config);
}

/**
 * @brief Configure the GPIO pin based on context parameters
 *
 * @param context DMDRVI context
 *
 * @return int 0 on success, non-zero on failure
 */
static int configure(dmdrvi_context_t context)
{
    int ret = dmgpio_port_init(
        context->config.port,
        context->config.pin,
        context->config.mode,
        context->config.pull,
        context->config.speed,
        context->config.alternate
    );

    if (ret == 0)
    {
        DMOD_LOG_INFO("GPIO P%s%d configured: mode=%s, pull=%s, speed=%s\n",
            port_to_string(context->config.port),
            context->config.pin,
            mode_to_string(context->config.mode),
            pull_to_string(context->config.pull),
            speed_to_string(context->config.speed));

        if (context->config.mode == dmgpio_mode_input)
        {
            context->state = dmgpio_port_read_pin(context->config.port, context->config.pin);
        }
    }
    else
    {
        DMOD_LOG_ERROR("Failed to configure GPIO P%s%d\n",
            port_to_string(context->config.port),
            context->config.pin);
    }
    return ret;
}

/**
 * @brief Read configuration from ioctl command
 *
 * @param context DMDRVI context
 * @param command IOCTL command
 * @param arg Argument pointer
 *
 * @return int 0 if this was a read command handled successfully, non-zero if not a read command
 */
static int read_configuration(dmdrvi_context_t context, int command, void* arg)
{
    switch (command)
    {
        case dmgpio_ioctl_cmd_get_state:
            *(dmgpio_pin_state_t*)arg = context->state;
            return 0;
        case dmgpio_ioctl_cmd_get_mode:
            *(dmgpio_mode_t*)arg = context->config.mode;
            return 0;
        case dmgpio_ioctl_cmd_get_pull:
            *(dmgpio_pull_t*)arg = context->config.pull;
            return 0;
        case dmgpio_ioctl_cmd_get_speed:
            *(dmgpio_speed_t*)arg = context->config.speed;
            return 0;
        default:
            return -1;
    }
}

/**
 * @brief Update configuration from ioctl command
 *
 * @param cfg Configuration structure to update
 * @param command IOCTL command
 * @param arg Argument pointer
 *
 * @return int 0 on success, non-zero on failure
 */
static int update_configuration(struct config* cfg, int command, void* arg)
{
    switch (command)
    {
        case dmgpio_ioctl_cmd_set_mode:
            cfg->mode = *(dmgpio_mode_t*)arg;
            return 0;
        case dmgpio_ioctl_cmd_set_pull:
            cfg->pull = *(dmgpio_pull_t*)arg;
            return 0;
        case dmgpio_ioctl_cmd_set_speed:
            cfg->speed = *(dmgpio_speed_t*)arg;
            return 0;
        default:
            return -EINVAL;
    }
}

/**
 * @brief Create a new DMDRVI context for the GPIO device
 *
 * @param config Dmini context with configuration data
 * @param dev_num Device number
 *
 * @return dmdrvi_context_t New context or NULL on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    dmdrvi_context_t context = (dmdrvi_context_t)Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (context == NULL)
    {
        DMOD_LOG_ERROR("Failed to allocate memory for DMDRVI context\n");
        return NULL;
    }

    Dmod_Memset(context, 0, sizeof(struct dmdrvi_context));
    context->magic = DMGPIO_CONTEXT_MAGIC;

    if (read_config_parameters(context, config) != 0)
    {
        DMOD_LOG_ERROR("Failed to read GPIO configuration\n");
        Dmod_Free(context);
        return NULL;
    }

    if (configure(context) != 0)
    {
        DMOD_LOG_ERROR("Failed to configure GPIO pin\n");
        Dmod_Free(context);
        return NULL;
    }

    DMOD_LOG_INFO("GPIO device created for P%s%d\n",
        port_to_string(context->config.port),
        context->config.pin);
    return context;
}

/**
 * @brief Free a DMDRVI context
 *
 * @param context DMDRVI context to free
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, void, _free, ( dmdrvi_context_t context ))
{
    if (is_valid_context(context))
    {
        dmgpio_port_deinit(context->config.port, context->config.pin);
        context->magic = 0;
        Dmod_Free(context);
    }
}

/**
 * @brief Open a device handle
 *
 * @param context DMDRVI context
 * @param flags Open flags
 *
 * @return void* Device handle
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, void*, _open, ( dmdrvi_context_t context, int flags ))
{
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmgpio_dmdrvi_open\n");
        return NULL;
    }
    return context;
}

/**
 * @brief Close a device handle
 *
 * @param context DMDRVI context
 * @param handle Device handle
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    // No specific action needed to close the GPIO device
}

/**
 * @brief Read from the device
 *
 * The data is returned in the format:
 * "port=<port>;pin=<pin>;state=<state>;mode=<mode>;pull=<pull>;speed=<speed>"
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer to read data into
 * @param size Size of the buffer
 *
 * @return size_t Number of bytes read
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, size_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size ))
{
    if (!is_valid_context(context))
    {
        return 0;
    }

    /* Refresh pin state before reading */
    context->state = dmgpio_port_read_pin(context->config.port, context->config.pin);

    int written = Dmod_SnPrintf(buffer, size,
        "port=%s;pin=%d;state=%d;mode=%s;pull=%s;speed=%s",
        port_to_string(context->config.port),
        context->config.pin,
        (int)context->state,
        mode_to_string(context->config.mode),
        pull_to_string(context->config.pull),
        speed_to_string(context->config.speed));
    return (written > 0) ? (size_t)written : 0;
}

/**
 * @brief Write to the device
 *
 * Writing "0" resets the pin, any other value sets it.
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer with data to write
 * @param size Number of bytes to write
 *
 * @return size_t Number of bytes written
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, size_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size ))
{
    if (!is_valid_context(context) || buffer == NULL || size == 0)
    {
        return 0;
    }

    const char* data = (const char*)buffer;
    dmgpio_pin_state_t state = (data[0] == '0') ? dmgpio_pin_reset : dmgpio_pin_set;
    dmgpio_port_write_pin(context->config.port, context->config.pin, state);
    context->state = state;
    return size;
}

/**
 * @brief Ioctl operation on the device
 *
 * List of supported commands can be found in #dmgpio_ioctl_cmd_t.
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param command Ioctl command
 * @param arg Argument for the command
 *
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    int ret = 0;
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmgpio_dmdrvi_ioctl\n");
        return -EINVAL;
    }

    if (command >= dmgpio_ioctl_cmd_max)
    {
        DMOD_LOG_ERROR("Invalid ioctl command %d in dmgpio_dmdrvi_ioctl\n", command);
        return -EINVAL;
    }

    if (command == dmgpio_ioctl_cmd_toggle)
    {
        dmgpio_port_toggle_pin(context->config.port, context->config.pin);
        context->state = (context->state == dmgpio_pin_reset) ? dmgpio_pin_set : dmgpio_pin_reset;
        return 0;
    }

    if (command == dmgpio_ioctl_cmd_reconfigure)
    {
        ret = configure(context);
        if (ret == 0)
        {
            DMOD_LOG_INFO("GPIO P%s%d reconfigured\n",
                port_to_string(context->config.port),
                context->config.pin);
        }
        return ret;
    }

    if (command == dmgpio_ioctl_cmd_set_state)
    {
        if (arg == NULL)
        {
            DMOD_LOG_ERROR("Null argument for set_state in dmgpio_dmdrvi_ioctl\n");
            return -EINVAL;
        }
        dmgpio_pin_state_t new_state = *(dmgpio_pin_state_t*)arg;
        dmgpio_port_write_pin(context->config.port, context->config.pin, new_state);
        context->state = new_state;
        return 0;
    }

    if (arg == NULL)
    {
        DMOD_LOG_ERROR("Null argument for ioctl command %d in dmgpio_dmdrvi_ioctl\n", command);
        return -EINVAL;
    }

    ret = read_configuration(context, command, arg);
    if (ret != 0)
    {
        struct config new_config;
        Dmod_Memcpy(&new_config, &context->config, sizeof(struct config));

        ret = update_configuration(&new_config, command, arg);
        if (ret == 0)
        {
            Dmod_Memcpy(&context->config, &new_config, sizeof(struct config));
            ret = configure(context);
        }
    }

    return ret;
}

/**
 * @brief Flush device buffers
 *
 * @param context DMDRVI context
 * @param handle Device handle
 *
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    return 0;
}

/**
 * @brief Get device statistics
 *
 * @param context DMDRVI context
 * @param handle Device handle
 * @param stat Pointer to dmdrvi_stat_t structure to fill
 *
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmgpio, int, _stat, ( dmdrvi_context_t context, void* handle, dmdrvi_stat_t* stat ))
{
    if (!is_valid_context(context) || stat == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters in dmgpio_dmdrvi_stat\n");
        return -EINVAL;
    }

    char info_buffer[256];
    int result = dmdrvi_dmgpio_read(context, handle, info_buffer, sizeof(info_buffer));
    if (result < 0)
    {
        return result;
    }
    stat->size = (uint32_t)result;
    stat->mode = 0666; // Read-write permissions
    return 0;
}
