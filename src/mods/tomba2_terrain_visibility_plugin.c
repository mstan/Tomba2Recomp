#include "mod_plugins.h"
#include "cpu_state.h"

#include <stdint.h>

/* Area zero's complete terrain blob is resident before this dispatcher runs.
 * Keep the selected cells first, then draw the rest of the resident area
 * through the original emitters. Neither camera selection nor the player's
 * visibility partition determines participation; primitive checks still do.
 *
 * Integration: call tomba2_terrain_visibility_activate() from the widescreen
 * activation and opt 8003D0BC into mod_function_entry_sites. The title's packet
 * allocator supplies the per-flip room check below; a shared aperture bounds
 * check alone cannot distinguish its two adjacent buffers. */
extern int tomba2_widescreen_packet_room(uint32_t bytes);

#define TERRAIN_DISPATCH 0x8003D0BCu
#define TERRAIN_RENDER 0x801401B8u
#define TERRAIN_RETURN 0x8003D0FCu
#define TERRAIN_OBJECT 0x800F2418u
#define TERRAIN_MASK_STRIDE 52u
#define TERRAIN_MAX_CELLS (TERRAIN_MASK_STRIDE * TERRAIN_MASK_STRIDE)
#define TERRAIN_BATCH_CELLS 254u
#define TERRAIN_DESCRIPTOR_BYTES 0x210u
#define TERRAIN_PACKET_LIMIT (1024u * 1024u)

typedef struct TerrainCell {
    uint16_t record;
    uint8_t chosen;
} TerrainCell;

static uint32_t terrain_descriptor;
static uint32_t terrain_peak_cells;
static uint32_t terrain_peak_packet_bound;

static int terrain_ram_span(uint32_t address, uint32_t bytes) {
    return address >= 0x80010000u && address <= 0x80200000u &&
           bytes <= 0x80200000u - address;
}

static int terrain_code_matches(void) {
    static const struct { uint32_t address, word; } words[] = {
        {0x8003D0BCu, 0x3C02800Cu},
        {0x8003D0C0u, 0x9043F870u},
        {0x80014EF0u, 0x8003D0F4u},
        {0x8003D0F4u, 0x0C05006Eu},
        {0x8003D0F8u, 0x00000000u},
        {0x801401B8u, 0x27BDFFD8u},
        {0x801401D4u, 0x90820006u},
        {0x801401E0u, 0x24910010u},
        {0x80140200u, 0x8C94000Cu},
        {0x80140260u, 0x00042080u},
        {0x80140270u, 0x0C04FEE2u},
        {0x80140274u, 0x320600FFu},
        {0x80140278u, 0x00108402u},
        {0x80140284u, 0x0C04FF96u},
        {0x80140288u, 0x320600FFu},
        {0x8013FB88u, 0x27BDFFE8u},
        {0x8013FE34u, 0x25AD0024u},
        {0x8013FE58u, 0x27BDFFE0u},
        {0x80140194u, 0x25AD002Cu},
        {0x8013FAF8u, 0x8C63C804u},
        {0x8013FB08u, 0x9442C800u},
        {0x8013FB10u, 0x00021100u},
        {0x8013FB18u, 0x00621024u},
    };
    for (unsigned i = 0; i < sizeof words / sizeof words[0]; ++i)
        if (psx_mod_read_word(words[i].address) != words[i].word) return 0;
    return 1;
}

/* Validate the complete source before making even the first guest draw. The
 * X-major grid records occupy a contiguous blob ending at the mask table;
 * this also proves uniqueness and bounds every original emitter read. */
static int terrain_plan(uint32_t object, TerrainCell *cells, uint16_t *order,
                        unsigned *draw_count,
                        unsigned *original_count, uint32_t *packet_bound) {
    const uint32_t grid = psx_mod_read_word(0x800ECF78u);
    const uint32_t masks = psx_mod_read_word(0x800ECF7Cu);
    const uint32_t next_asset = psx_mod_read_word(0x800ECF80u);
    const unsigned region = psx_mod_read_half(0x8014C800u);
    const unsigned selected = psx_mod_read_byte(object + 6u);
    if (!terrain_ram_span(object, TERRAIN_DESCRIPTOR_BYTES) ||
        !terrain_ram_span(grid, 4u) || (grid & 3u) ||
        masks <= grid || (masks & 3u) || next_asset <= masks ||
        !terrain_ram_span(grid, masks - grid) ||
        !terrain_ram_span(masks, next_asset - masks) ||
        !region || region > 15u || selected > TERRAIN_BATCH_CELLS ||
        psx_mod_read_word(object + 12u) != grid ||
        psx_mod_read_word(0x8014C804u) != masks) return 0;

    const unsigned width = psx_mod_read_half(grid);
    const unsigned height = psx_mod_read_half(grid + 2u);
    if (!width || !height || width > TERRAIN_MASK_STRIDE ||
        height > TERRAIN_MASK_STRIDE ||
        psx_mod_read_half(object + 8u) != width ||
        psx_mod_read_half(object + 10u) != height) return 0;
    const unsigned slots = width * height;
    const uint32_t table_bytes = 4u + 2u * slots;
    const uint32_t mask_bytes = (height - 1u) * TERRAIN_MASK_STRIDE + width;
    if (table_bytes > masks - grid || mask_bytes > next_asset - masks) return 0;

    uint32_t source_end = (grid + table_bytes + 3u) & ~3u;
    uint32_t worst_bytes = 0;
    unsigned count = 0;
    for (unsigned i = 0; i < slots; ++i) {
        const uint16_t record = psx_mod_read_half(grid + 4u + 2u * i);
        if (record == 0xFFFFu) continue;
        const uint32_t source = grid + 4u * record;
        if (source != source_end || source >= masks || masks - source < 4u)
            return 0;
        const uint32_t counts = psx_mod_read_word(source);
        if (counts & 0xFF00FF00u) return 0;
        const unsigned triangles = counts & 255u;
        const unsigned quads = (counts >> 16) & 255u;
        const uint32_t source_bytes = 4u + 36u * triangles + 44u * quads;
        const uint32_t packet_bytes = 40u * triangles + 52u * quads;
        if (source_bytes > masks - source ||
            packet_bytes > TERRAIN_PACKET_LIMIT - worst_bytes) return 0;
        source_end += source_bytes;
        worst_bytes += packet_bytes;
        cells[count++] = (TerrainCell){record, 0};
    }
    if (!count || source_end != masks) return 0;

    unsigned ordered = 0;
    for (unsigned i = 0; i < selected; ++i) {
        const uint16_t record = psx_mod_read_half(object + 16u + 2u * i);
        unsigned j = 0;
        while (j < count && cells[j].record != record) ++j;
        if (j == count) return 0;
        if (!cells[j].chosen) {
            cells[j].chosen = 1;
            order[ordered++] = (uint16_t)j;
        }
    }
    *original_count = ordered;
    for (unsigned i = 0; i < count; ++i) {
        if (!cells[i].chosen) {
            cells[i].chosen = 1;
            order[ordered++] = (uint16_t)i;
        }
    }
    *draw_count = ordered;
    *packet_bound = worst_bytes;
    return 1;
}

static int tomba2_draw_current_area(CPUState *cpu, uint32_t address) {
    if (!cpu || (address & 0x1FFFFFFFu) != (TERRAIN_DISPATCH & 0x1FFFFFFFu) ||
        psx_mod_widescreen_view_x_margin() <= 0 ||
        psx_mod_read_byte(0x800BF870u) != 0 ||
        cpu->gpr[4] != TERRAIN_OBJECT) return 0;
    if (!terrain_descriptor) {
        psx_mod_counter_add("tomba2.terrain.no_descriptor", 1);
        return 0;
    }
    if (!terrain_code_matches()) {
        psx_mod_counter_add("tomba2.terrain.code_reject", 1);
        return 0;
    }

    TerrainCell cells[TERRAIN_MAX_CELLS];
    uint16_t order[TERRAIN_MAX_CELLS];
    unsigned drawn, original;
    uint32_t packet_bound;
    if (!terrain_plan(cpu->gpr[4], cells, order, &drawn, &original,
                      &packet_bound)) {
        psx_mod_counter_add("tomba2.terrain.layout_reject", 1);
        return 0;
    }
    if (!drawn) return 0;
    if (!tomba2_widescreen_packet_room(packet_bound)) {
        psx_mod_counter_add("tomba2.terrain.packet_reject", 1);
        return 0;
    }

    /* The private descriptor is never exposed to updates or the camera
     * selector. Every batch keeps the original grid and at most 254 cells.
     * The guest-call service restores caller GPR/PC/HI/LO while retaining
     * the renderer's GTE, packet, OT and timing effects. */
    for (unsigned i = 0; i < 4u; ++i)
        psx_mod_write_word(terrain_descriptor + 4u * i,
                           psx_mod_read_word(cpu->gpr[4] + 4u * i));
    uint32_t result = 0;
    for (unsigned first = 0; first < drawn; first += TERRAIN_BATCH_CELLS) {
        unsigned batch = drawn - first;
        if (batch > TERRAIN_BATCH_CELLS) batch = TERRAIN_BATCH_CELLS;
        for (unsigned i = 0; i < batch; ++i)
            psx_mod_write_half(terrain_descriptor + 16u + 2u * i,
                               cells[order[first + i]].record);
        psx_mod_write_byte(terrain_descriptor + 6u, (uint8_t)batch);
        result = psx_mod_call_guest(cpu, TERRAIN_RENDER, TERRAIN_RETURN,
                                    terrain_descriptor, cpu->gpr[5],
                                    cpu->gpr[6], cpu->gpr[7]);
        psx_mod_counter_add("tomba2.terrain.batches", 1);
    }
    cpu->gpr[2] = result;
    psx_mod_counter_add("tomba2.terrain.frames", 1);
    psx_mod_counter_add("tomba2.terrain.cells", drawn);
    psx_mod_counter_add("tomba2.terrain.added_cells", drawn - original);
    if (drawn > terrain_peak_cells) {
        psx_mod_counter_add("tomba2.terrain.max_cells", drawn - terrain_peak_cells);
        terrain_peak_cells = drawn;
    }
    if (packet_bound > terrain_peak_packet_bound) {
        psx_mod_counter_add("tomba2.terrain.max_packet_bound", packet_bound - terrain_peak_packet_bound);
        terrain_peak_packet_bound = packet_bound;
    }
    return 1;
}

void tomba2_terrain_visibility_activate(void) {
    if (!terrain_descriptor)
        terrain_descriptor = psx_mod_alloc_guest_memory(TERRAIN_DESCRIPTOR_BYTES, 16u);
    if (!terrain_descriptor)
        psx_mod_counter_add("tomba2.terrain.allocation_failed", 1);
}

PSX_MOD_CONSTRUCTOR(tomba2_register_terrain_visibility) {
    (void)psx_mod_register_function_filter_plugin(
        "tomba2.widescreen.16-9", TERRAIN_DISPATCH, tomba2_draw_current_area);
    (void)psx_mod_register_function_filter_plugin(
        "tomba2.widescreen.21-9", TERRAIN_DISPATCH, tomba2_draw_current_area);
    (void)psx_mod_register_function_filter_plugin(
        "tomba2.widescreen.adaptive", TERRAIN_DISPATCH, tomba2_draw_current_area);
}
