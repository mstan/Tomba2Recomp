#include "mod_plugins.h"

/* The original generated loader, decoder and sound-transfer bodies remain
 * the sole implementation in REFERENCE. This callback acknowledges the
 * shared product catalog without preparing assets or installing a dispatch
 * interceptor. It never fabricates completion of a guest request. */
static void activate(void) {}

PSX_MOD_CONSTRUCTOR(tomba2_register_seamless) {
    (void)psx_mod_register_activation_plugin("tomba2.seamless", activate);
}
