#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmgpio_lease.h"
#include "dmgpio_port.h"
#include <errno.h>
#include <string.h>
#define GPIO_LEASE_MAGIC 0x47504c53U
struct dmgpio_lease
{
    uint32_t magic;
    dmgpio_port_t port;
    dmgpio_pins_mask_t mask;
    dmgpio_edge_t edge;
    void *user;
};
static void dispatch(void *arg, dmgpio_port_t port, dmgpio_pins_mask_t pins, dmgpio_pins_mask_t state)
{
    (void)port; (void)pins;
    dmgpio_lease_t p = arg;
    if (p->magic == GPIO_LEASE_MAGIC && p->edge) p->edge(p->user, (state & p->mask) != 0);
}
dmod_dmgpio_api_declaration(1.0, void, _pin_write, (dmgpio_lease_t p, bool high))
{
    if (p && p->magic == GPIO_LEASE_MAGIC)
        dmgpio_port_set_pins_state(p->port, p->mask, high ? dmgpio_pins_state_all_high : dmgpio_pins_state_all_low);
}
dmod_dmgpio_api_declaration(1.0, bool, _pin_read, (dmgpio_lease_t p))
{
    return p && p->magic == GPIO_LEASE_MAGIC && dmgpio_port_get_high_state_pins(p->port, p->mask);
}
dmod_dmgpio_api_declaration(1.0, void, _pin_release, (dmgpio_lease_t p))
{
    if (!p || p->magic != GPIO_LEASE_MAGIC) return;
    Dmod_EnterCritical();
    if (p->edge) dmgpio_port_set_interrupt_trigger(p->port, p->mask, dmgpio_int_trigger_off);
    dmgpio_port_remove_interrupt_handler(p->port, p);
    dmgpio_port_set_mode(p->port, p->mask, dmgpio_mode_input);
    dmgpio_port_set_pins_unused(p->port, p->mask);
    p->magic = 0;
    Dmod_ExitCritical();
    Dmod_Free(p);
}
static int configure_pin(dmgpio_lease_t p, dmgpio_mode_t mode, uint8_t af, bool high)
{
    int r = dmgpio_port_set_power(p->port, 1);
    if (!r) r = dmgpio_port_set_output_circuit(p->port, p->mask, dmgpio_output_circuit_push_pull);
    if (!r) dmgpio_pin_write(p, high); /* preload before enabling output */
    if (!r && mode == dmgpio_mode_alternate) r = dmgpio_port_set_alternate_function(p->port, p->mask, af);
    if (!r) r = dmgpio_port_set_speed(p->port, p->mask, dmgpio_speed_maximum);
    if (!r) r = dmgpio_port_set_mode(p->port, p->mask, mode);
    if (!r && p->edge) r = dmgpio_port_add_interrupt_handler(p->port, p->mask, dispatch, p);
    if (!r && p->edge) r = dmgpio_port_set_interrupt_trigger(p->port, p->mask, dmgpio_int_trigger_both_edges);
    return r;
}
dmod_dmgpio_api_declaration(1.0, int, _pin_acquire, (int16_t pin, dmgpio_mode_t mode, uint8_t af, bool high, dmgpio_edge_t edge, void *user, dmgpio_lease_t *out))
{
    if (!out || pin < 0 || pin >= 176 || af > 15) return -EINVAL;
    *out = NULL;
    dmgpio_lease_t p = Dmod_Malloc(sizeof(*p));
    if (!p) return -ENOMEM;
    *p = (struct dmgpio_lease){GPIO_LEASE_MAGIC, pin / 16, 1U << (pin % 16), edge, user};
    Dmod_EnterCritical();
    int r = dmgpio_port_claim_pins(p->port, p->mask);
    Dmod_ExitCritical();
    if (r) { Dmod_Free(p); return -EBUSY; }
    Dmod_EnterCritical();
    r = configure_pin(p, mode, af, high);
    Dmod_ExitCritical();
    if (r) { dmgpio_pin_release(p); return r; }
    *out = p;
    return 0;
}
