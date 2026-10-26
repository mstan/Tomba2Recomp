/* Test the real game predicate, including fail-closed overlay qualification. */
#include "mod_plugins.h"
#undef PSX_MOD_CONSTRUCTOR
#define PSX_MOD_CONSTRUCTOR(name) static void name(void)
#include "../src/mods/tomba2_widescreen_plugin.c"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static uint32_t words[2] = {0x3C021F80u, 0x90420136u};
static uint8_t mode;
uint32_t psx_mod_read_word(uint32_t address) {
    assert(address == 0x8003BB50u || address == 0x8003BB54u);
    return words[(address - 0x8003BB50u) / 4u];
}
uint8_t psx_mod_read_byte(uint32_t address) {
    assert(address == 0x1F800136u);
    return mode;
}
int main(void) {
    mode = 0; assert(!tomba2_native_menu_scene()); /* gameplay/cutscenes */
    mode = 1; assert(tomba2_native_menu_scene());  /* first Pause frame */
    mode = 2; assert(tomba2_native_menu_scene());  /* journal/inventory */
    mode = 3; assert(!tomba2_native_menu_scene());
    mode = 255; assert(!tomba2_native_menu_scene());
    mode = 1; words[0] ^= 1; assert(!tomba2_native_menu_scene());
    words[0] ^= 1; words[1] ^= 1; assert(!tomba2_native_menu_scene());
    words[1] ^= 1; assert(tomba2_native_menu_scene());
    mode = 0; assert(!tomba2_native_menu_scene()); /* immediate recovery */
    puts("PASS Tomba 2 menu aspect and instruction guards");
    return 0;
}
