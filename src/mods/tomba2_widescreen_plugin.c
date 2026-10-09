#include "mod_plugins.h"
#include "cpu_state.h"
#include "gte_nclip_stats.h"

/*
 * Projection, backdrop, overlay-cull, and resident-object participation hooks
 * remain in game.toml. The mod activates their presentation aspect before
 * renderer startup and removes the verified render queue's radial far gates.
 * Every hook is inert at the authentic 4:3 baseline.
 */
#define TOMBA2_PACKET_BYTES (1024u * 1024u)
static uint32_t tomba2_packet_arena;
static uint32_t tomba2_packet_peak;
extern void tomba2_terrain_visibility_activate(void);
extern void tomba2_actor_queue_activate(void);

int tomba2_widescreen_packet_room(uint32_t bytes) {
    if (!tomba2_packet_arena || psx_mod_widescreen_view_x_margin() <= 0)
        return 0;
    const uint32_t flip = psx_mod_read_byte(0x1F800135u);
    if (flip > 1u) return 0;
    const uint32_t cursor = psx_mod_read_word(0x800BF544u) & 0x00FFFFFFu;
    const uint32_t base = (tomba2_packet_arena & 0x00FFFFFFu) +
                          flip * TOMBA2_PACKET_BYTES;
    return cursor >= base && cursor <= base + TOMBA2_PACKET_BYTES &&
           bytes <= base + TOMBA2_PACKET_BYTES - cursor;
}

static int tomba2_packet_cursor_extended(void) {
    uint32_t cursor = psx_mod_read_word(0x800BF544u) & 0x00FFFFFFu;
    uint32_t base = tomba2_packet_arena & 0x00FFFFFFu;
    return tomba2_packet_arena && cursor >= base &&
           cursor < base + 2u * TOMBA2_PACKET_BYTES;
}

static void tomba2_reset_packet_arena(CPUState *cpu, uint32_t address) {
    if (!cpu || !tomba2_packet_arena ||
        psx_mod_widescreen_view_x_margin() <= 0) return;

    /* The stock loop resets the primitive cursor to BFE68 + flip*14000.
     * Its second 80 KiB buffer ends at E7E68: enlarged distant geometry
     * overflowed it into Tomba's header at E7E80 during Zippo's intro.
     * Redirect this original SW's value, preserving its delay slot, both
     * display buffers, the original OT and the original DMA submission.
     * The shared allocation service supplies CPU/PGXP/24-bit-DMA transport
     * and save-state storage; ordinary expansion memory cannot carry tags. */
    uint32_t flip = psx_mod_read_byte(0x1F800135u);
    if ((address & 0x1FFFFFFFu) != 0x00050CB4u || flip > 1u ||
        cpu->gpr[3] != 0x000BFE68u + flip * 0x14000u ||
        cpu->gpr[20] != 0x800C0000u ||
        psx_mod_read_word(0x80050CA4u) != 0x2442FE68u ||
        psx_mod_read_word(0x80050CA8u) != 0x00621821u ||
        psx_mod_read_word(0x80050CACu) != 0x00651824u ||
        psx_mod_read_word(0x80050CB0u) != 0x0C01E22Bu) {
        psx_mod_counter_add("tomba2.widescreen.packet_guard_reject", 1);
        return;
    }
    if (tomba2_packet_cursor_extended()) {
        uint32_t used = ((psx_mod_read_word(0x800BF544u) & 0x00FFFFFFu) -
                         (tomba2_packet_arena & 0x00FFFFFFu)) % TOMBA2_PACKET_BYTES;
        if (used > tomba2_packet_peak) {
            psx_mod_counter_add("tomba2.widescreen.packet_peak_bytes", used - tomba2_packet_peak);
            tomba2_packet_peak = used;
        }
        if (used > 0x14000u)
            psx_mod_counter_add("tomba2.widescreen.packet_over_stock_frames", 1);
    }
    cpu->gpr[3] = (tomba2_packet_arena + flip * TOMBA2_PACKET_BYTES) & 0x00FFFFFFu;
    psx_mod_counter_add("tomba2.widescreen.packet_frames", 1);
}

static void tomba2_visible_far_models(CPUState *cpu, uint32_t address) {
    uint32_t expected_compare;
    if (!cpu || cpu->gpr[2] != 0 ||
        psx_mod_widescreen_view_x_margin() <= 0 ||
        !tomba2_packet_cursor_extended()) return;

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
    (void)psx_mod_register_instruction_plugin(plugin,
        0x80050CB4u, 0xAE83F544u, tomba2_reset_packet_arena);
}

static void tomba2_native_projection_activate(void) {
    /* Only the two audited terrain consumers may recover a positive face
     * whose integer area rounded to zero. Model winding stays architectural;
     * the shared runtime also verifies the complete word and GTE provenance. */
    static const uint32_t zero_sites[] = {0x8013FC40u, 0x8013FF14u};
    static const uint32_t zero_words[] = {0x1840007Bu, 0x1840009Eu};
    psx_mod_set_native_wide_nclip_zero_sites(zero_sites, zero_words, 2);
    /* Activation may replay when the mod plan changes. The shared allocator
     * is monotonic; retain this allocation and its observed high-water mark. */
    if (!tomba2_packet_arena)
        tomba2_packet_arena = psx_mod_alloc_gpu_dma_memory(2u * TOMBA2_PACKET_BYTES, 32u);
    if (!tomba2_packet_arena)
        psx_mod_counter_add("tomba2.widescreen.packet_allocation_failed", 1);
    tomba2_terrain_visibility_activate();
    tomba2_actor_queue_activate();
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
