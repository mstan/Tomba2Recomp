# Tomba2Recomp

Static recompilation of **Tomba! 2 - The Evil Swine Return (USA)** (serial
**SCUS-94454**) to native code, built on the shared **psxrecomp** framework —
the same toolchain that powers TombaRecomp, ApeEscapeRecomp and MegaManX6Recomp.

## Status

Scaffolded 2026-06-21. Boot EXE extracted, headerless Ghidra dump prepared,
`game.toml` / `CMakeLists.txt` mirror the Ape Escape minimal template. First
build/boot bring-up in progress.

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
  -DPSX_DEBUG_TOOLS=ON -DPSX_LAUNCHER=OFF
cmake --build build-master --target psx-runtime -j 16
```

The boot EXE is a small loader; the bulk of the game streams from disc as code
overlays at runtime (same architecture as Tomba! 1).

---

<p align="center">
  <sub><b>R.A.I.D. — Retro AI Development</b> · a Discord for AI-assisted retro reverse-engineering, decomp &amp; recomp</sub>
</p>

<p align="center">
  <a href="https://discord.gg/Ad9BwSzctP"><img src=".github/raid-discord.png" alt="Join the Retro AI Development (R.A.I.D.) Discord" width="200"></a>
</p>
