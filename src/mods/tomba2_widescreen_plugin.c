#include "mod_plugins.h"
#include "cpu_state.h"

/*
 * Projection, backdrop, overlay-cull, and resident-object participation hooks
 * remain in game.toml. The mod activates their presentation aspect before
 * renderer startup and removes the verified render queue's radial far gates.
 * Every hook is inert at the authentic 4:3 baseline.
 */
static void tomba2_visible_far_models(CPUState *cpu, uint32_t address) {
    uint32_t expected_compare;
    if (!cpu || cpu->gpr[2] != 0 ||
        psx_mod_widescreen_view_x_margin() <= 0) return;

    /* FUN_8007712C receives an already resident object and camera delta,
     * rejects by radial distance, then applies its view cone and appends to
     * the bounded render queue. The old man (type 9) was invisible at a
     * distance of 5716 despite lying inside the cone: its cutoff was 5120.
     * Widening only that cutoff in a fixed-camera A/B restores his model.
     *
     * Bypass only the far predicate. Keep the actual distance for the cone,
     * the earlier near test, the model-type queue capacities and all later
     * primitive/depth tests. There is no replacement arbitrary far radius.
     * The instruction service guards the complete branch and its live word;
     * also qualify the preceding comparison and common function entry. */
    switch (address & 0x1FFFFFFFu) {
    case 0x0007724Cu: expected_compare = 0x28821401u; break;
    case 0x000772E8u: expected_compare = 0x28821C01u; break;
    case 0x00077394u: expected_compare = 0x28821C01u; break;
    case 0x00077428u: expected_compare = 0x28821001u; break;
    case 0x000774BCu: expected_compare = 0x28821001u; break;
    case 0x00077550u: expected_compare = 0x28821A01u; break;
    default: return;
    }
    if (psx_mod_read_word(address - 4u) != expected_compare ||
        psx_mod_read_word(0x8007712Cu) != 0x00051400u) return;
    cpu->gpr[2] = 1;
    psx_mod_counter_add("tomba2.widescreen.far_gate_bypass", 1);
}

static void tomba2_register_far_model_sites(const char *plugin) {
    static const struct { uint32_t address, expected; } sites[] = {
        {0x8007724Cu, 0x14400003u},
        {0x800772E8u, 0x1040FFDAu},
        {0x80077394u, 0x1040FFAFu},
        {0x80077428u, 0x1040FF8Au},
        {0x800774BCu, 0x1040FF65u},
        {0x80077550u, 0x1040FF40u},
    };
    for (unsigned i = 0; i < sizeof(sites) / sizeof(sites[0]); ++i)
        (void)psx_mod_register_instruction_plugin(plugin,
            sites[i].address, sites[i].expected, tomba2_visible_far_models);
}

static void tomba2_native_projection_activate(void) {
    psx_mod_set_native_wide_projection_correction(1);
    psx_mod_set_native_wide_near_clip(1);
}

static void tomba2_widescreen_16_9_activate(void) {
    tomba2_native_projection_activate();
    (void)psx_mod_set_fixed_display_aspect(16u, 9u);
}

static void tomba2_widescreen_21_9_activate(void) {
    tomba2_native_projection_activate();
    (void)psx_mod_set_fixed_display_aspect(21u, 9u);
}

static void tomba2_widescreen_adaptive_activate(void) {
    tomba2_native_projection_activate();
    (void)psx_mod_set_fixed_display_aspect(16u, 9u);
    (void)psx_mod_set_adaptive_display_aspect(21u, 9u);
}

PSX_MOD_CONSTRUCTOR(tomba2_register_widescreen_plugins) {
    tomba2_register_far_model_sites("tomba2.widescreen.16-9");
    tomba2_register_far_model_sites("tomba2.widescreen.21-9");
    tomba2_register_far_model_sites("tomba2.widescreen.adaptive");
    (void)psx_mod_register_activation_plugin(
        "tomba2.widescreen.16-9", tomba2_widescreen_16_9_activate);
    (void)psx_mod_register_activation_plugin(
        "tomba2.widescreen.21-9", tomba2_widescreen_21_9_activate);
    (void)psx_mod_register_activation_plugin(
        "tomba2.widescreen.adaptive", tomba2_widescreen_adaptive_activate);
}
