#include "tomba2_terrain_execution.h"
#include "mod_plugins.h"

uint32_t tomba2_terrain_emit_batch(CPUState *cpu, uint32_t descriptor,
                                  int additional_scenery) {
    (void)additional_scenery;
    return psx_mod_call_guest(cpu, TOMBA2_TERRAIN_RENDER,
        TOMBA2_TERRAIN_RETURN, descriptor, cpu->gpr[5], cpu->gpr[6], cpu->gpr[7]);
}
