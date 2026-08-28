#include "mod_plugins.h"

/*
 * Tomba 2 normally produces a new display image at roughly 30 Hz while its
 * guest VBlank, input, audio, and scheduler continue at their original rates.
 * These callbacks only select the OpenGL presentation blend cadence.
 */
static void tomba2_frame_rate_set(unsigned frames_per_second) {
    (void)psx_mod_set_frame_interpolation_blend(
        PSX_MOD_FRAME_INTERPOLATION_MOTION_ADAPTIVE);
    (void)psx_mod_set_frame_interpolation(frames_per_second);
}

static void tomba2_frame_rate_60_activate(void) {
    tomba2_frame_rate_set(60u);
}

static void tomba2_frame_rate_120_activate(void) {
    tomba2_frame_rate_set(120u);
}

static void tomba2_frame_rate_144_activate(void) {
    tomba2_frame_rate_set(144u);
}

static void tomba2_frame_rate_165_activate(void) {
    tomba2_frame_rate_set(165u);
}

static void tomba2_frame_rate_display_activate(void) {
    tomba2_frame_rate_set(0u);
}

PSX_MOD_CONSTRUCTOR(tomba2_register_frame_rate_plugins) {
    (void)psx_mod_register_activation_plugin(
        "tomba2.framerate.60", tomba2_frame_rate_60_activate);
    (void)psx_mod_register_activation_plugin(
        "tomba2.framerate.120", tomba2_frame_rate_120_activate);
    (void)psx_mod_register_activation_plugin(
        "tomba2.framerate.144", tomba2_frame_rate_144_activate);
    (void)psx_mod_register_activation_plugin(
        "tomba2.framerate.165", tomba2_frame_rate_165_activate);
    (void)psx_mod_register_activation_plugin(
        "tomba2.framerate.uncapped", tomba2_frame_rate_display_activate);
}
