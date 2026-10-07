# USA parity source preparation

Tracking: `beads-eio.2.13`, within campaign `beads-eio.3.277`.
This is source preparation, not a compiled, packaged or accepted candidate.
The owner supplies final playtest acceptance.

## Preserved foundation and build contract

The branch starts from USA release `82ade2a` (v0.1.0), whose parents are
`ef255b3` and `24ab59f`. It retains the accepted resident/decode implementation,
fresh original-disc AOT release pipeline and removal of the slow bundled debug
menu. The earlier campaign audit's `5fc54c8` is not the release base.
Framework pin: `8f95b599997399a476062e99634ddbb67e5c8731`, CODEGEN 18.
Current native libraries and generated trees from older framework pins require
fresh generation and audits; renaming their namespaces is not sufficient.

`PSX_EXECUTION_PROFILE=ENHANCED` selects the native resource preparation and
USA dispatch adapters. `REFERENCE` selects a catalog callback that installs no
interceptor and prepares no assets: the unchanged generated game/overlays
execute their original reader, decoder and sample-transfer bodies. This is a
maintained source path; its new build and live qualification are pending.
The resident-loading Mods checkbox remains an independent ENHANCED product
preference. `TOMBA2_SEAMLESS_RETAIL` no longer selects an implementation.
The Italian catalog never enables these USA adapters; Italian enhancements and
regional caller proofs remain follow-up work (`beads-eio.2.12`).

The shared implementation manifest records `tomba2-resident-loader` with
contract `scus94454-resource-worker-v1`. It hashes selected adapter sources,
catalogs and the existing contract document. Both packaging paths validate the
requested profile and bind the actual staged binary to its execution sidecar.
Linux binding follows linuxdeploy. Future Windows signing must precede binding.
Separate build/cache directories are required; cross-profile savestate and
netplay compatibility are not promised. Ordinary memory cards remain game data.

## Caller inventory

Disc SHA-256:
`8e9568388155787384c3166a5934a495fbff3fa57154ab0489b0f214fe05a8ab`.
MAIN.EXE SHA-256:
`cb580f5bd42895b1c452dffe53e1c8cb639a029e0afec194ae197fdfda9721ed`.
These are recorded identities from the accepted release, not newly hashed here.
See [the existing evidence](SEAMLESS_LOADING_SPIKE.md) for the original-disc
oracle, guards and earlier performance/audio receipts.

| Boundary | Inputs/guards | Caller-visible publication |
|---|---|---|
| Reader `8001DB8C` / `8001DC40` | a0 destination, a1 LBA, a2 byte count; original MAIN prologue, catalog range, bounded guest RAM, idle worker where required | Resource bytes; v0 original size; scratchpad completion cells `1F8001F0..1F8001F8`, `1F800284/288`; reader status `800BE0E0/E6/E8/EA`; executable/overlay invalidation |
| Spawn `80051F14`, terminate `80051FB4` | a0=1, a1 one of `80044F58/8004514C/800452C0`; parent RA `80044C50`; original extent/TCB/stack layout; idle workers and normal sound mode | Worker initialization and resource effects finish before parent observes completion; original worker stack/context restored; TCB completion flags published; other workers keep original scheduling |
| Decoder `80044D8C` | a0 texture header/width, a1 destination, a2 encoded source, a3 size; original opcode; cached encoded bytes and width must match | Exact original output bytes and original decoded length in v0; bounded destination and executable invalidation |
| Sample transfer `80099150` | caller RA `80096A00`; a0 source, a1 size; original opcode, ordinary transfer mode, rounded length and SPU bounds | SPU destination/mode and DMA bytes; status `800AC654/658/65C/638`; v0 input size; header allocation, voice metadata and music commands stay original |

Inputs outside these guards reach the original loader in ENHANCED. A failed
preparation installs no adapter. This existing scoped behavior is preserved,
not a live HLE/LLE selector. The reference callback reports no fabricated guest
success; it simply leaves the original dispatch chain in place.

## Enhancement inventory and remaining gates

| Feature | Source evidence | Remaining work |
|---|---|---|
| OpenGL / internal resolution | OpenGL retained; dev and USA release configs now request shared `1080p` | Fresh build and effective-resolution receipt; settings may override the preset |
| Native scene interpolation | Current `tomba2_frame_rate_plugin.c` only selects motion-adaptive image blending | Discover original draw-only span, then checkpoint/replay it through shared APIs. `80050CB0/80050CC0` are known loop/debug detour sites, not qualified replay boundaries. Exclude CD/resource, input, audio and simulation work; qualify HOLD/native passes at Display refresh and appropriate FMV/menu/pause/transition handling |
| PGXP / world filtering / perspective | Builtin PGXP available; USA release currently geometry correction off and nearest filtering, preserving validated flavor-0 AOT | Fresh matching PGXP/flavor AOT, world/UI classification and captures; stable world filtering, perspective, seams/jitter/texel bleed across texture depths. Do not enable a flavor without native libraries for it |
| Adaptive 4:3..32:9 | Existing projection, flat/phase backdrops, screen-X guards, angle hooks, activation guards and opcode-checked model cones; adaptive activation currently capped at 21:9 | Implement and qualify 32:9 participation, background and HUD coverage; changing a cap alone is insufficient. Preserve fixed 24/40/28 model queues, 0xFE terrain-list cap and gameplay-sensitive X/Z activation; validate camera edges and late areas |
| HUD / authored screens | `nw_hud_corners=false`; BIOS/FMV/true 2D authored at 4:3 | Classify HUD and confirm anchoring, backgrounds and transitions at each aspect |
| Distance / fog / subdivision | Existing wide cones deliberately retain original vertical/distance gates; no qualified title distance/fog/subdivision replacement found | Attribute actual draw cost and original thresholds; document applicability before changing distance or removing subdivisions |
| Resident loading / startup | All 34 immutable files and 280 exact decoded blocks; ~31 MiB; existing native first-run cache and worker adapters | Compare REFERENCE vs ENHANCED startup, cold/warm preparation, first transitions and revisits with turbo off; verify audio and native coverage after CODEGEN 18 regeneration. Music/speech/FMV streaming stays original |
| Multiplayer | Tomba 2 is single-player | No multiplayer addition in scope |
| Packages | Original-disc fresh AOT, audit and deterministic Windows ZIP; pinned Linux tools; new execution binding | Rebuild/audit CODEGEN 18 for both platforms, inspect contents and hashes, reproduce candidates, owner acceptance |

Generated boot declarations do not cover the gameplay draw loop. Local release
overlay source files are private and older-codegen evidence only. No original
instruction proof or playable replay boundary was established in this pass;
no native interpolation or full view coverage is claimed.

The available private `cg4_fa2a5191/4E7FD27E_patched.c` was inspected narrowly.
Its generated `80050B08` body includes initialization and worker spawning;
`80050CB0` calls `800788AC`, `80050CB8` calls `80051E60`, and `80050CC0`
calls `80080F6C`. `800788AC` mutates persistent halfword/list state and calls
`800524B4` / `8005229C`. `80051E60` writes the current cooperative thread cell
at `1F800138` and starts from TCB `801FE000`. This evidence rejects those broad
loop sites as an assumed pure draw boundary. Next investigation must trace the
render-producing worker/packet calls inside the scheduler, confirm their
original-disc instructions, and choose a smaller span that owns draw outputs
without advancing cooperative simulation or persistent game state.

## Source checks and next validation commands

The compiler-free fixture exercises the real title CMakeLists, replaces only
heavy runtime target creation, and uses the real shared execution helper. Both
ENHANCED and REFERENCE configure successfully, checking USA and Italian targets
link exactly one selected family and publish it in their manifests. PowerShell
and Bash packager syntax and `git diff --check` pass. No compiler, game, disc
hash, AOT generation, download or package construction ran in this pass.

Run the source fixture with native Windows executables, separately per profile:

```powershell
& 'C:/msys64/mingw64/bin/cmake.exe' -S tests/source_profile -B _source-check/enhanced -G Ninja -DPSX_EXECUTION_PROFILE=ENHANCED -DPSXRECOMP_ROOT=F:/Projects/psxrecomp/_wt-parity-hle-20261006
& 'C:/msys64/mingw64/bin/cmake.exe' -S tests/source_profile -B _source-check/reference -G Ninja -DPSX_EXECUTION_PROFILE=REFERENCE -DPSXRECOMP_ROOT=F:/Projects/psxrecomp/_wt-parity-hle-20261006
```

Do not build the fixture; its compile/link rules deliberately fail. When the
coordinator grants heavy-work slots, initialize the pinned framework and UI
submodules and supply the original USA disc at the config path. Then use the
existing packagers without `-SkipRegen`; they regenerate boot code and require
fresh original-disc extraction/AOT audits. At most two compile jobs and one
game process run across the campaign:

```powershell
./tools/package_release.ps1 -Variant usa -ExecutionProfile ENHANCED -BuildDir build-parity-usa-enhanced -Jobs 2 -Version parity-candidate
./tools/package_release.ps1 -Variant usa -ExecutionProfile REFERENCE -BuildDir build-parity-usa-reference -Jobs 2 -Version reference-candidate
```

```sh
bash tools/package_appimage.sh --variant usa --execution-profile ENHANCED --build-dir /path/to/tomba2-enhanced --jobs 2 --version parity-candidate
bash tools/package_appimage.sh --variant usa --execution-profile REFERENCE --build-dir /path/to/tomba2-reference --jobs 2 --version reference-candidate
```

Before promotion, build a separate debug runtime with isolated saves and an
unused port (candidate proposal: 4715), compare supported caller outputs and
side effects against REFERENCE, and measure startup/loads/frame time separately.
Re-run `tomba2_seamless_prepare_test` and the independent 280-block MIPS oracle
from the existing document. Use natural transitions and revisits; the development
debug menu changes render code and forces native-guard misses.
Confirm game-started/boot-turbo gates before restoring any private test snapshot.

After a native draw span is actually implemented, record correctness with
`PSX_RENDER_PASS_VERIFY=1`, then relaunch without it for performance:

```text
python FRAMEWORK/tools/qualify_render_passes.py --port 4715 --case A00-moving-camera --seconds 10 --purpose correctness --executable BUILD/Tomba2Recomp.exe --output PRIVATE/replay-correctness.json
python FRAMEWORK/tools/qualify_render_passes.py --port 4715 --case A00-moving-camera --seconds 10 --purpose performance --executable BUILD/Tomba2Recomp.exe --output PRIVATE/replay-performance.json
python FRAMEWORK/tools/load_probe.py --port 4715 run --secs 60
```

Use screenshots of the enhanced buffer at 4:3, 16:9, 21:9 and 32:9 during
movement, edge activation, scene backgrounds and HUD, plus FMV, menus, pauses,
fades, door/wing travel, death/retry and late-game event variants. Keep metrics,
package hashes and feedback private. Beads remains open through owner acceptance;
public pushes, merges and releases are not authorized.

## Resumed first-review build

The Windows review build now targets framework e5e2dca8 (20b0 plus the shared
Ninja fix for generated contract inputs), UI03d58aa0, and unchanged CODEGEN18
/d1867bb4. Earlier source inventories above retain their historical pins.
Per the owner, build ENHANCED and its native cache, then use the short manual
playtest before broadening tests. This preparation is not gameplay acceptance.

## First Windows owner-review candidate

The USA ENHANCED runtime built from title `ba501b4`, framework `e5e2dca8` and
recomp-ui `03d58aa0`. The 231-task runtime build succeeded. The executable is
`build-campaign-enhanced/Tomba2Recomp.exe`, SHA-256
`2d04d30ff0aaca84f119b67175165bd556f56e680d5fd99ed446d64a41f82396`.
Its imports are Windows system libraries. Execution identity is
`70673a07c57c12b94e9954d5ccab189134e97353920cb5b72dac6ce999fcb095`, with
the actual `tomba2-resident-loader` / `scus94454-resource-worker-v1` selected.

Fresh original-disc AOT verified 29 images / 54 recipes and staged 81 native
pairs (14,619 manifest rows), namespace `cg18_d1867bb4_gc309154ca_f0`.
Pair and original-byte guard audits passed; complete native coverage is not
claimed. Evidence: `build-aot-review/disc-aot-kwgg5byl` and `review-aot.log`.
Local fallback compiler paths are explicit with two workers, preserving the
canonical overlay config hash.

Owner launcher: `F:/Projects/psxrecomp/parity-review-20261006/Play-Tomba2.ps1`,
with executable receipt, private cards and debug port 4691. Check opening-area
movement, background layers, dialogue, widescreen, pause, door/area transitions
and revisits; try death/retry or wing travel when convenient. No new automated
gameplay pass ran. Native scene interpolation, full 32:9 (current cap is 21:9),
full loading coverage, Linux artifacts and final owner acceptance remain open.
