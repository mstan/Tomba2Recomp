cmake_minimum_required(VERSION 3.20)
project(Tomba2Recomp C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp-v4" CACHE PATH
    "Path to psxrecomp")

include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

# Split-TU build: the recompiler writes the recompiled game as N
# SCUS_944.54_full_NN.c shards (plus a shared SCUS_944.54_decls.h) instead of
# one monolithic SCUS_944.54_full.c, so every shard compiles as its own
# translation unit in parallel. CONFIGURE_DEPENDS re-globs on every build so
# a shard-count change (regen) is picked up without a manual re-configure.
file(GLOB GAME_GENERATED_FULL CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/SCUS_944.54_full_*.c")
set(GAME_GENERATED_DISPATCH
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/SCUS_944.54_dispatch.c")

psxrecomp_add_runtime_target(psx-runtime
    GAME_GENERATED_FULL_C "${GAME_GENERATED_FULL}"
    GAME_GENERATED_DISPATCH_C "${GAME_GENERATED_DISPATCH}"
    # (OUTPUT_NAME set below: the exe ships as Tomba2Recomp.exe so process
    # management — taskkill, port collisions, task manager — never confuses
    # this title with other psxrecomp games' psx-runtime.exe instances.)
    # 4515: Tomba2's OWN debug port. 4500 collides with the SNES mmx_cosim
    # processes (they bind 127.0.0.1:4500 too and eat connections); other
    # taken ports: 4370/4380 psxrecomp-v4, 4470 Tomba1, 4510 Tsumu, 4520 MMX5.
    DEBUG_PORT 4515
    WINDOW_TITLE "Tomba! 2 Recompiled"
    # No DEFAULT_BIOS_PATH: an absolute value would bake a build-machine path
    # into the binary (the framework refuses those and falls back to relative
    # "bios/SCPH1001.BIN"). Dev trees resolve it via the exe-dir upward search
    # (psxrecomp-v4/bios); user installs use bios.cfg / the picker.
    DEFAULT_GAME_CONFIG_PATH "game.toml"
)

# Distinct process name (see comment above).
set_target_properties(psx-runtime PROPERTIES OUTPUT_NAME "Tomba2Recomp")

# Shared recomp-ui Dear ImGui launcher. Wires in the PSX-themed launcher UI
# (see main.cpp RECOMP_LAUNCHER gate) instead of the framework's default RmlUi
# launcher. recomp-ui is a real git submodule (github.com/mstan/recomp-ui,
# pinned via the recomp-ui gitlink); locally it is a directory junction to the
# working checkout so builds stay offline and reproducible like everything else.
include("${CMAKE_SOURCE_DIR}/recomp-ui/recomp_ui.cmake")
recomp_target_launcher_ui(psx-runtime
    BOXART "${CMAKE_SOURCE_DIR}/recomp/launcher/boxart.tga"
    PAD    "${CMAKE_SOURCE_DIR}/recomp/launcher/pad.tga"
    BRAND  "${CMAKE_SOURCE_DIR}/recomp/launcher/brand.tga"
)
