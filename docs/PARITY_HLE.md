# Tomba 2 USA native interpolation review

Updated 2026-10-07. Tracking `beads-eio.2.13`; owner acceptance remains open.

The hash-bound `Play-Tomba2.ps1` in the campaign review directory launches
`build-native-review/Tomba2Recomp.exe`, with private cards and debug port 4691.
Source milestone `458d85b`, shared framework `0b225784`, recomp-ui `03d58aa0`.
This replaces the earlier blending-only candidate.

## Implemented and checked

- OpenGL and 1080p internal target, PGXP compiled into the USA primary build.
- Native Scene Interpolation / Display refresh rebuilds the two MAIN scene
  aggregates (`8003F9A8..8003FA14`, `8003FA44..8003FA9C`) with interpolated
  GTE projections. Shared draw snapshots, late-OT preservation and draw-bank
  retargeting restore all guest state after intermediate draws.
- Submission occurs at the main-loop PutDispEnv after the real VBlank wait.
  Capture respects the 224-line NTSC scanout inside its 240-line buffers.
  Shared forced-span control prevents following the draw return into audio.
- Existing native-wide projection/culling/backgrounds and resident loading /
  texture decoding / sample transfer remain available. The local review
  selects widescreen, interpolation and Skip FMVs. Native interpolation now
  defaults on per the owner's latest decision; other optional features retain
  their source defaults. Final performance review follows a system restart.
- Fresh original-disc AOT: 54 recipes, 81 native pairs, 14,619 manifest rows,
  namespace `cg18_d1867bb4_gcb4db33e1_f2`. All pairs and original-byte guards
  passed. Evidence: `build-aot-review/disc-aot-42sg678l`.
- Five-second moving attract scene: 262 native passes, 627 intermediate
  presents, 262 state verifications, zero mismatch/leak/abort/watchdog/blending.
  Receipt: `receipts/Tomba2.native-attract-moving.json` in the review directory.
  Enhanced screenshot shows complete geometry and the authored demo overlay.

The ENHANCED build selects `tomba2-resident-loader`, contract
`scus94454-resource-worker-v1`. REFERENCE selects the original generated
reader/decoder/sample-transfer bodies and does not prepare the resident cache.
The maintained source path remains; this pass did not rebuild its full matrix.
The Italian title does not enable these USA adapters or this USA replay plugin.

## Human review and remaining limits

Use Native Scene Interpolation / Display refresh, OpenGL / 1080p and the
widescreen option. Check running/jumping, camera turns, foreground/background
layers, HUD and dialogue, pause, door/area transitions and a revisit. Check
Skip FMVs independently, and listen for intact music during loading.

Native replay has bounded attract-scene evidence, not all-area acceptance or a
performance claim. The view still caps at 21:9; 32:9 participation/HUD expansion,
stable world/UI filtering qualification, Linux packages, extended native
performance and owner acceptance remain open. Separate correctness readbacks
from performance measurements. Ordinary memory cards remain game data;
cross-profile savestate compatibility is not promised.

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
