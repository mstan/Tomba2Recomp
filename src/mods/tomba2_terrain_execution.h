#ifndef TOMBA2_TERRAIN_EXECUTION_H
#define TOMBA2_TERRAIN_EXECUTION_H

#include "cpu_state.h"
#include <stdint.h>

/* Guarded USA area-zero packet producer. Both builds execute the same guest
 * callee with the same descriptor/arguments and retain its complete effects.
 * ENHANCED removes guest-time cost only for the added resident scenery. */
#define TOMBA2_TERRAIN_RENDER 0x801401B8u
#define TOMBA2_TERRAIN_RETURN 0x8003D0FCu
uint32_t tomba2_terrain_emit_batch(CPUState *cpu, uint32_t descriptor,
                                  int additional_scenery);

#endif
