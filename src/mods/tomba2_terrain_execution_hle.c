#include "tomba2_terrain_execution.h"
#include "mod_plugins.h"

uint32_t tomba2_terrain_emit_batch(CPUState *cpu, uint32_t descriptor,
                                  int additional_scenery) {
    if (!additional_scenery)
        return psx_mod_call_guest(cpu, TOMBA2_TERRAIN_RENDER,
            TOMBA2_TERRAIN_RETURN, descriptor, cpu->gpr[5], cpu->gpr[6], cpu->gpr[7]);
    /* Pure CPU/RAM/GTE packet work: DMA submission remains at the original
     * caller. Bound the span so an unsupported device wait cannot freeze.
     * The shared service keeps packet/OT/precision effects and caller state. */
    int charged = 0;
    uint32_t result = psx_mod_call_guest_uncharged(cpu, TOMBA2_TERRAIN_RENDER,
        TOMBA2_TERRAIN_RETURN, descriptor, cpu->gpr[5], cpu->gpr[6], cpu->gpr[7],
        4000000u, &charged);
    psx_mod_counter_add(charged ? "tomba2.terrain.hle_budget_fallback" :
                                "tomba2.terrain.hle_batches", 1);
    return result;
}
