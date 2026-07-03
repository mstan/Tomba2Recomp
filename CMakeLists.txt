cmake_minimum_required(VERSION 3.20)
project(Tomba2Recomp C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp-v4" CACHE PATH
    "Path to psxrecomp")

include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

set(GAME_GENERATED_FULL
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/SCUS_944.54_full.c")
set(GAME_GENERATED_DISPATCH
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/SCUS_944.54_dispatch.c")

psxrecomp_add_runtime_target(psx-runtime
    GAME_GENERATED_FULL_C "${GAME_GENERATED_FULL}"
    GAME_GENERATED_DISPATCH_C "${GAME_GENERATED_DISPATCH}"
    # 4515: Tomba2's OWN debug port. 4500 collides with the SNES mmx_cosim
    # processes (they bind 127.0.0.1:4500 too and eat connections); other
    # taken ports: 4370/4380 psxrecomp-v4, 4470 Tomba1, 4510 Tsumu, 4520 MMX5.
    DEBUG_PORT 4515
    WINDOW_TITLE "Tomba! 2 Recompiled"
    DEFAULT_BIOS_PATH "${PSXRECOMP_ROOT}/bios/SCPH1001.BIN"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
)
