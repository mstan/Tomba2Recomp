#include "mod_plugins.h"
#include "cpu_state.h"

#include <stdint.h>

extern int tomba2_widescreen_packet_room(uint32_t bytes);

/* These resident fire/smoke families use a linked draw list, separate from
 * the main actor queues. Their far test also suppresses animation. Qualify
 * the complete caller family before changing only that render predicate. */
static void tomba2_visible_far_effects(CPUState *cpu, uint32_t address) {
    if (!cpu || (address & 0x1FFFFFFFu) != 0x0002B314u ||
        cpu->gpr[2] != 0 || cpu->gpr[5] <= 7168u ||
        cpu->gpr[16] != 0x1F8000D0u ||
        !tomba2_widescreen_packet_room(1u)) return;
    const uint32_t object = cpu->gpr[20], sp = cpu->gpr[29];
    if (object < 0x80010000u || object > 0x801FFFD0u ||
        sp < 0x80010000u || sp > 0x801FFFD8u ||
        psx_mod_read_byte(object + 12u) != 6u ||
        psx_mod_read_byte(object + 11u) != 32u) return;

    static const struct { uint32_t address, expected; } common[] = {
        {0x8002B278u,0x27BDFFD8u}, {0x8002B280u,0x0080A021u},
        {0x8002B290u,0xAFBF0024u}, {0x8002B2ECu,0x0C01DFECu},
        {0x8002B2F4u,0x3045FFFFu}, {0x8002B2F8u,0x28A20200u},
        {0x8002B308u,0x1440001Eu}, {0x8002B310u,0x28A21C01u},
        {0x8002B318u,0x00930018u}, {0x8002B368u,0x0082202Au},
        {0x8002B374u,0xA2820001u}, {0x8002B380u,0x00001021u},
    };
    for (unsigned i = 0; i < sizeof common / sizeof common[0]; ++i)
        if (psx_mod_read_word(common[i].address) != common[i].expected) return;

    static const struct {
        uint32_t caller, delay, continuation;
        uint32_t update, update_entry, draw, draw_entry;
    } families[] = {
        {0x8013C4F8u,0x02002021u,0x14400009u,
         0x8013C3F4u,0x27BDFFE8u,0x80027CB4u,0x27BDFFE8u},
        {0x8013C7ACu,0x02602021u,0x14400007u,
         0x8013C538u,0x27BDFFD8u,0x800281ECu,0x27BDFFC8u},
        {0x8013CDACu,0x02002021u,0x0804F371u,
         0x8013C9C0u,0x27BDFFE8u,0x8013CDD4u,0x27BDFF98u},
    };
    const uint32_t ra = psx_mod_read_word(sp + 0x24u);
    for (unsigned i = 0; i < sizeof families / sizeof families[0]; ++i) {
        if (ra != families[i].caller + 8u ||
            psx_mod_read_word(object + 0x1Cu) != families[i].update ||
            psx_mod_read_word(object + 0x18u) != families[i].draw) continue;
        if (psx_mod_read_word(families[i].caller) != 0x0C00AC9Eu ||
            psx_mod_read_word(families[i].caller + 4u) != families[i].delay ||
            psx_mod_read_word(ra) != families[i].continuation ||
            psx_mod_read_word(families[i].update) != families[i].update_entry ||
            psx_mod_read_word(families[i].draw) != families[i].draw_entry) return;
        cpu->gpr[2] = 1;
        psx_mod_counter_add("tomba2.widescreen.effect_far_bypass", 1);
        return;
    }
}

PSX_MOD_CONSTRUCTOR(tomba2_register_effect_visibility) {
    (void)psx_mod_register_instruction_plugin("tomba2.widescreen.16-9",
        0x8002B314u, 0x1040001Au, tomba2_visible_far_effects);
    (void)psx_mod_register_instruction_plugin("tomba2.widescreen.21-9",
        0x8002B314u, 0x1040001Au, tomba2_visible_far_effects);
    (void)psx_mod_register_instruction_plugin("tomba2.widescreen.adaptive",
        0x8002B314u, 0x1040001Au, tomba2_visible_far_effects);
}
