# Tomba2Recomp

> _This recompilation is a **byproduct of developing
> [psxrecomp](https://github.com/mstan/psxrecomp)** — the games are the proving ground, the framework is the goal.
> **These are in-development previews, not finished ports — expect rough
> edges**, and depth will keep landing over months, not days. My time for any
> one title is limited, so I ask for your patience. Contributions are welcome —
> testing, issues, and PRs to the game or framework all help and will
> accelerate this game's polish. More on the why at:
> [Recomp + AI: 5 Months Later »](https://1379.tech/recomp-ai-5-months-later/)_

Static recompilation of **Tomba! 2 - The Evil Swine Return (USA)** (serial
**SCUS-94454**) to native code, built on the shared **psxrecomp** framework —
the same toolchain that powers TombaRecomp, ApeEscapeRecomp and MegaManX6Recomp.

## Status

Windows and Linux preview builds support the US disc (SCUS-94454) and Italian PAL disc
(SCES-02686). Both include native code prepared from all 22 original area files,
plus shared overlays, before gameplay. The owner accepted performance in water
temple and Kujara Ranch. Full native coverage and a full playthrough remain
unproven; interpreter and runtime-compilation fallback remain available.

See [AOT release preparation](docs/AOT_RELEASE.md) for reproduction and validation,
and the framework's [AOT sharding guide](https://github.com/RetroPortingToolKit/psxrecomp/blob/master/docs/AOT_SHARDING.md)
for the reusable workflow. Full game decompilation is not required for these
verified overlay layouts.

## Playing

Release builds include the MIT-licensed OpenBIOS from PCSX-Redux. No external
BIOS is required: select your legally obtained Tomba! 2 disc image in the
launcher and press Launch. The optional BIOS row accepts the exact supported
retail dump; clear it to return to bundled OpenBIOS.

Tomba 2's mods live on the launcher's **Mods** page. Seamless Loading is enabled
by default for the US disc, replacing the generic CD Speed and Fast Loading
options. Adaptive widescreen and 1080p internal rendering are also enabled by default.
Interpolation is disabled and hidden; Skip FMVs remains optional. Italian retains its existing loading options.

## Layout

- `tomba2/` — disc image (bin/cue), extracted boot EXE `SCUS_944.54`,
  `SYSTEM.CNF`. Local only (gitignored).
- `ghidra/` — headerless dump + import notes (`instructions.txt`).
- `seeds/` — function-start seeds for the recompiler.
- `generated/` — recompiler output C (regenerated locally, gitignored).
- `psxrecomp-v4` — junction to a psxrecomp worktree (the shared framework).
- `game.toml` — game identity, recompiler + runtime config.

## Build

```sh
# regenerate game C (master-flavor recompiler):
../psxrecomp/recompiler/build/psxrecomp-game.exe --config game.toml
# configure + build the runtime:
cmake -S . -B build-master -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe \
  -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe \
  -DPSX_DEBUG_TOOLS=ON
cmake --build build-master --target psx-runtime -j 16
```

SDL3 is the default host backend. To build the explicit SDL2 compatibility
fallback, add `-DPSX_SDL_BACKEND=SDL2` to the configure command above. CMake
prints the selected backend and never silently changes it.

The boot EXE is a small loader; the bulk of the game streams from disc as code
overlays at runtime (same architecture as Tomba! 1).

## Built-in mods

Enable **Tomba 2 Widescreen** on the Mods page and select 16:9, 21:9, or
Adaptive. Adaptive follows the live window or fullscreen aspect from 4:3 up to
21:9. Resizing wider reveals more of the world instead of stretching a fixed
image; BIOS, FMVs, menus, and other true-2D screens remain pillarboxed at their
authored 4:3 aspect.

Interpolation is withheld from the bundled catalog after performance and
visual review. The original scene loop retains its authored cadence.

**Skip FMVs** mutes and rapidly advances streamed XA/MDEC movies plus the
silent, RAM-preloaded Whoopee Camp logo. The game still runs its normal movie
completion and teardown path.

**Seamless Loading** prepares immutable resources from the
player's disc once, caches them on disk, and keeps them in host memory during
play. Verified resource workers complete without the loading-screen loop;
gameplay, cutscenes, and audio retain their normal pacing. Disable it to use
the original loader. See [the spike assessment](docs/SEAMLESS_LOADING_SPIKE.md)
for tested routes and remaining coverage.

The experimental **Debug Menu** is withheld from the bundled catalog because
its patched render loop lacks native coverage and causes severe slowdown,
including with resident loading disabled. Its source is retained under
`mods/sources` and its package under [mods/development](mods/development).

## License

PolyForm Noncommercial 1.0.0. See `LICENSE`.

This repository contains no Tomba! 2 game assets or disc data. Release packages
include OpenBIOS under the MIT notice in `bios/OpenBIOS.LICENSE`; they contain
no retail PlayStation BIOS.

---

<p align="center">
  <sub><b>R.A.I.D. — Retro AI Development</b> · a Discord for AI-assisted retro reverse-engineering, decomp &amp; recomp</sub>
</p>

<p align="center">
  <a href="https://discord.gg/Ad9BwSzctP"><img src=".github/raid-discord.png" alt="Join the Retro AI Development (R.A.I.D.) Discord" width="200"></a>
</p>
