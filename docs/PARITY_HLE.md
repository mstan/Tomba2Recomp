# Tomba 2 USA serial enhancement review

Updated 2026-10-08. Tracking `beads-eio.2.13` and visibility repair
`beads-eio.2.14`; owner acceptance remains open.

The owner accepts saving the current progress and confirms the old man now
stays visible. The far-right pig/pole, sign left of the old man and tree behind
the burning building still appear late or disappear with camera angle.
Whole-area residency and removal of camera render culling are proposed next
investigations, not implemented or validated features at this checkpoint.

The hash-bound `Play-Tomba2.ps1` in the campaign review directory launches
`build-viewport-review/Tomba2Recomp.exe`, with private cards and debug port 4691.
Shared packed-X culling support is on `fix/tomba2-packed-x-20261008`
(`7e7c94c24bb1b32fb539b6074cd008c3a04897d7`); recomp-ui remains `03d58aa0`.
The execution receipt records the exact compiled source and executable identity.

## Implemented and checked

- OpenGL and 1080p internal target, PGXP compiled into the USA primary build.
- Native interpolation is omitted from the build and catalog, following the
  owner's decision to avoid it for the remaining serial reviews.
- Adaptive native widescreen is enabled by default, up to the existing 21:9
  support limit. Existing resident loading, texture decoding and sample
  transfer remain enabled; generic CD-speed and host pacing controls are hidden.
- The earlier village old man and Evil Pig reproduction lost triangles at the original
  horizontal viewport bounds. Three full-opcode-guarded SLTU sites at
  `801466A0`, `801466A8` and `801466B0` now test the adaptive horizontal range.
  The shared emitter and interpreter use the same predicate, with exact 4:3
  behavior and original-opcode fallback. Vertical, backface, depth and queue
  limits remain in force. See the actor investigation document for attribution.
- The new reproduction also identified whole-model rejection: old man
  `800FC8D8`, type 9, was 5716 units from the camera and inside its cone, but
  `FUN_8007712C` rejected him at its 5120-unit radial cutoff. Fixed-camera,
  single-cutoff A/B restores his head/arm above the hill; restoring the cutoff
  removes them. Six guarded instruction hooks cover the seven far comparisons
  in this resident-model render-queue function, without a replacement far
  radius. Near tests, the actual distance used by cone math, queue capacities,
  actor lifetime and primitive/depth checks stay intact. These hooks are inert
  at 4:3 and are not the retired global distance/cone experiment.
- Admitting distant models exposed an original 80 KiB drawing-buffer overflow
  during Zippo's intro. Traced quad writes damaged Tomba's bone counts before
  an invalid matrix write damaged the BIOS exception vector. An exact guarded
  hook at the original frame reset now selects paired 1 MiB GPU-DMA buffers;
  far admission requires the expanded cursor. A native diagnostic replay
  exceeded the stock budget in 107 frames (peak 86,728 bytes) without the
  corruption or fatal halt. The final normal config/cache rebuild also passed:
  all 81 native pairs audited, old man visible at distance 6104.22, and clean
  intro-to-gameplay completion without diagnostic code patches.
- Exact projection, near clipping and exact NCLIP are enabled for native wide
  rendering. Five full-word-guarded GTE rejection sites ignore horizontal
  saturation in wide view while retaining vertical, divide, depth and MAC
  errors; the original signed predicate runs at 4:3.
- The water/cloud seam was a shared compositor defect: the canonical 4:3
  center was copied over an already stretched phase/flat backdrop. Those
  existing backdrop opt-ins now require the full composite. The playable
  village capture shows continuous water/clouds with the default fast path.
  See [the viewport attribution and checks](VIEWPORT_REPAIR.md).
- Native overlay inputs reuse the established original-disc extraction in
  `build-aot-review/disc-aot-42sg678l`. Updated artifacts must be regenerated and
  audited against those inputs; an old cache namespace cannot be relabeled.
- The current candidate uses bundled OpenBIOS after fresh-boot validation.
  Retail-BIOS savestates are used only in the isolated diagnostic reproduction.

The ENHANCED build selects `tomba2-resident-loader`, contract
`scus94454-resource-worker-v1`. REFERENCE selects the original generated
reader/decoder/sample-transfer bodies and does not prepare the resident cache.
The maintained source path remains; this pass did not rebuild its full matrix.
The Italian title does not enable these USA adapters or this USA replay plugin.

## Human review and remaining limits

Use OpenGL / 1080p, adaptive widescreen, texture correction and Seamless Loading.
In the starting village, check the old man and Evil Pig near the ladder/shop
while moving the camera, then make an area transition and return. Listen for
intact music during fast loading and check HUD/dialogue placement. No
interpolation option should appear. Use normal saves for OpenBIOS; the old
retail-BIOS diagnostic savestate is not a cross-profile compatibility promise.
The old man's lower body is genuinely behind the foreground hill at the
diagnostic doorway camera; the regression was the missing visible head/arm.

The view still caps at 21:9; 32:9 participation/HUD expansion,
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
