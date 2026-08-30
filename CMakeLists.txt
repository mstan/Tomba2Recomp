cmake_minimum_required(VERSION 3.20)
project(Tomba2Recomp C CXX)
include(CTest)

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

file(GLOB TOMBA2_MOD_PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/*.c")

# Isolated-worktree process-identity overrides. A sibling investigation may
# build this same game repo from a DIFFERENT worktree at the same time; without
# a distinct exe name / debug port the two builds' runtimes collide on process
# name (taskkill / Task Manager confusion) and on the TCP debug port. Empty by
# default so an ordinary configure uses OUTPUT_NAME "Tomba2Recomp", DEBUG_PORT
# 4515.
set(PSXRECOMP_GAME_EXE_NAME_OVERRIDE "" CACHE STRING
    "Override this game's runtime OUTPUT_NAME (default: Tomba2Recomp)")
set(PSXRECOMP_GAME_DEBUG_PORT_OVERRIDE "" CACHE STRING
    "Override this game's default TCP debug port (default: 4515)")

set(_tomba2_exe_name "Tomba2Recomp")
if(PSXRECOMP_GAME_EXE_NAME_OVERRIDE)
    set(_tomba2_exe_name "${PSXRECOMP_GAME_EXE_NAME_OVERRIDE}")
endif()
set(_tomba2_debug_port 4515)
if(PSXRECOMP_GAME_DEBUG_PORT_OVERRIDE)
    set(_tomba2_debug_port "${PSXRECOMP_GAME_DEBUG_PORT_OVERRIDE}")
endif()

psxrecomp_add_runtime_target(psx-runtime
    GAME_GENERATED_FULL_C "${GAME_GENERATED_FULL}"
    GAME_GENERATED_DISPATCH_C "${GAME_GENERATED_DISPATCH}"
    EXTRAS_SOURCES ${TOMBA2_MOD_PLUGIN_SOURCES}
    # (OUTPUT_NAME set below: the exe ships as Tomba2Recomp.exe so process
    # management — taskkill, port collisions, task manager — never confuses
    # this title with other psxrecomp games' psx-runtime.exe instances.
    # PSXRECOMP_GAME_EXE_NAME_OVERRIDE swaps this for an isolated build.)
    # 4515: Tomba2's OWN debug port. 4500 collides with the SNES mmx_cosim
    # processes (they bind 127.0.0.1:4500 too and eat connections); other
    # taken ports: 4370/4380 psxrecomp-v4, 4470 Tomba1, 4510 Tsumu, 4520 MMX5.
    # PSXRECOMP_GAME_DEBUG_PORT_OVERRIDE swaps this for an isolated build.
    DEBUG_PORT ${_tomba2_debug_port}
    WINDOW_TITLE "Tomba! 2 Recompiled"
    # No DEFAULT_BIOS_PATH: an absolute value would bake a build-machine path
    # into the binary (the framework refuses those and falls back to relative
    # "bios/SCPH1001.BIN"). Dev trees resolve it via the exe-dir upward search
    # (psxrecomp-v4/bios); user installs use bios.cfg / the picker.
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    LAUNCHER_BOXART "${CMAKE_SOURCE_DIR}/recomp/launcher/boxart.tga"
)

# Distinct process name (see comment above).
set_target_properties(psx-runtime PROPERTIES OUTPUT_NAME "${_tomba2_exe_name}")

# Ship Tomba 2's trusted, game-owned mods beside the runtime. The authentic
# 4:3/non-interpolated presentation and stock guest code remain the disabled
# baseline.
set(TOMBA2_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")

if(EXISTS "${TOMBA2_PRELOADED_MODS}/packages")
    add_custom_command(TARGET psx-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${TOMBA2_PRELOADED_MODS}"
            "$<TARGET_FILE_DIR:psx-runtime>/mods"
        COMMENT "Preloading the Tomba 2 mod catalog")
endif()

if(BUILD_TESTING)
    add_executable(tomba2_preloaded_mods_test
        tests/test_preloaded_mods.cpp
        "${PSXRECOMP_ROOT}/runtime/src/mod_packages.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/crc32.c"
        "${PSXRECOMP_ROOT}/runtime/src/psx_sha256.c"
    )
    target_include_directories(tomba2_preloaded_mods_test PRIVATE
        "${PSXRECOMP_ROOT}/runtime/include"
        "${PSXRECOMP_ROOT}/recompiler/lib/toml11"
    )
    add_test(
        NAME tomba2_preloaded_mods_test
        COMMAND "$<TARGET_FILE:tomba2_preloaded_mods_test>"
                "${TOMBA2_PRELOADED_MODS}"
    )
endif()

# ---------------------------------------------------------------------------
# Italian PAL release (Tombi! 2 - The Evil Swine Return, SCES-02686).
# Same runtime, same mod plugins; only the game image differs. The recompiler
# writes generated_ita/SCES_026.86_* shards from game_ita.toml.
file(GLOB GAME_ITA_GENERATED_FULL CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/generated_ita/SCES_026.86_full_*.c")
set(GAME_ITA_GENERATED_DISPATCH
    "${CMAKE_CURRENT_SOURCE_DIR}/generated_ita/SCES_026.86_dispatch.c")

set(PSXRECOMP_GAME_ITA_EXE_NAME_OVERRIDE "" CACHE STRING
    "Override the Italian runtime OUTPUT_NAME (default: Tombi2Recomp-ita)")
set(PSXRECOMP_GAME_ITA_DEBUG_PORT_OVERRIDE "" CACHE STRING
    "Override the Italian runtime TCP debug port (default: 4516)")

set(_tomba2_ita_exe_name "Tombi2Recomp-ita")
if(PSXRECOMP_GAME_ITA_EXE_NAME_OVERRIDE)
    set(_tomba2_ita_exe_name "${PSXRECOMP_GAME_ITA_EXE_NAME_OVERRIDE}")
endif()
set(_tomba2_ita_debug_port 4516)
if(PSXRECOMP_GAME_ITA_DEBUG_PORT_OVERRIDE)
    set(_tomba2_ita_debug_port "${PSXRECOMP_GAME_ITA_DEBUG_PORT_OVERRIDE}")
endif()

psxrecomp_add_runtime_target(psx-runtime-ita
    GAME_GENERATED_FULL_C "${GAME_ITA_GENERATED_FULL}"
    GAME_GENERATED_DISPATCH_C "${GAME_ITA_GENERATED_DISPATCH}"
    EXTRAS_SOURCES ${TOMBA2_MOD_PLUGIN_SOURCES}
    DEBUG_PORT ${_tomba2_ita_debug_port}
    WINDOW_TITLE "Tombi! 2 (Italian) Recompiled"
    DEFAULT_GAME_CONFIG_PATH "game_ita.toml"
    LAUNCHER_BOXART "${CMAKE_SOURCE_DIR}/recomp/launcher/boxart.tga"
)

set_target_properties(psx-runtime-ita PROPERTIES OUTPUT_NAME
    "${_tomba2_ita_exe_name}")

if(EXISTS "${TOMBA2_PRELOADED_MODS}/packages")
    add_custom_command(TARGET psx-runtime-ita POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${TOMBA2_PRELOADED_MODS}"
            "$<TARGET_FILE_DIR:psx-runtime-ita>/mods"
        COMMENT "Preloading the Tombi 2 (ITA) mod catalog")
endif()
