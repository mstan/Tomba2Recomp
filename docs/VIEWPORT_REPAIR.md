# Tomba 2 viewport repair, 2026-10-08

Owner report: an obvious water/cloud discontinuity at the old 4:3 boundary,
and the old man missing until Tomba moves down into the village. Investigation
used the actual loaded instructions, render-function entry/exit records,
camera data and fixed-camera A/B. Beads: `beads-eio.2.14`; shared support:
`beads-eio.3.313`. Human acceptance remains pending.

## Whole-model disappearance

The old man is resident object `800FC8D8`, type 9, callback `8011D578`.
`FUN_8007712C` clears its visibility byte, tests radial distance and a camera
cone, then appends accepted models to a bounded render queue. At camera
`(5330, -1372, 2352)`, forward `(1980, 696, 3516)`, the old man's delta was
`(4620, 352, 3348)`. Distance was approximately 5716 and cosine was 926 Q10:
inside the cone, beyond the original 5120-unit cutoff at `80077248`.

Changing only that cutoff in the private same-camera A/B changed visibility
from 0 to 1 and restored the visible head/arm at the house doorway. Restoring
the cutoff removed them again. The hill legitimately occludes the lower body.
This identifies a far-distance rejection rather than a queue-size assumption.

The widescreen plugin qualifies six instruction branches by complete live
opcode, preceding comparison and common function-entry word. On rejection it
sets only the comparison result to acceptance; it does not change the actual
distance used by subsequent cone calculations or install an arbitrary larger
radius. The final branch covers both default-type and mode-four comparisons.

| Branch | Expected complete word |
|---|---|
| `8007724C` | `14400003` |
| `800772E8` | `1040FFDA` |
| `80077394` | `1040FFAF` |
| `80077428` | `1040FF8A` |
| `800774BC` | `1040FF65` |
| `80077550` | `1040FF40` |

All three wide choices register the sites. At the authentic 4:3 baseline the
callback is inert. Existing near, cone, queue, depth, actor lifetime and room
loading checks remain; this does not load other rooms or remove occlusion.

The private all-far A/B measured queue highwater `19/19/7` against capacities
`24/40/28`, with zero aspect-queue rejects. That camera-only check did not
exercise the earlier Zippo intro with the far hooks active from boot.

## Drawing storage exposed by the far-model repair

The first production replay halted during Zippo's intro. A write trace proves
the original primitive arena overflowed: at frame 6662 the quad producer's
stores at `80146838/3C/58/74` wrote GPU packet words across Tomba's actor header
`800E7E80..8C`. Its bone-count word changed from `0F001111` to `00505050`.
The next frame's animation matrix walker trusted those counts, wrote through
an invalid destination at `00000071`, and damaged the BIOS exception vector.
The subsequent fatal syscall was a consequence of this corruption. Disabling
only the far-hook comparison qualification let the same route complete.

Stock packet buffers start at `800BFE68` and `800D3E68`, each `14000` bytes
(80 KiB). The second ends at `800E7E68`, immediately before Tomba's header.
The new exact-word hook at `80050CB4` (`AE83F544`) redirects the per-frame
cursor reset to two 1 MiB buffers from `psx_mod_alloc_gpu_dma_memory`. It
preserves the original flip index, ordering tables, packet commands, delay
slot and DMA submission. The shared service transports the enlarged packet
addresses through CPU access, PGXP and 24-bit DMA tags, and snapshots the
allocated bytes. Activation reuses the allocation when the mod plan replays.

The hook also guards the original base calculation, flip bit, destination
register and nearby instruction words. Far acceptance requires the live
cursor to be within the expanded arena; allocation or reset qualification
failure keeps the original predicate. At 4:3 the original reset is unchanged.
Counters record packet frames, peak bytes, frames exceeding the stock budget,
allocation failure and reset-guard rejection. The small actual-plugin contract
checks all 21 registrations, both buffers, peak accounting, repeated activation,
4:3 identity, register preservation and failed-arena/failed-code fallback.

Attribution: `receipts/Tomba2.primitive-arena-cause.json`, backed by the full
`Tomba2.diagnose-far-player.json` write trace and the far-disabled same-route
receipt. A temporary single-module native replay validated the new storage.
The final production cache was then regenerated normally under its new config.

The final private replay reached normal gameplay and the distant doorway:
`receipts/Tomba2.arena-confirmed-playable.json` records old-man visibility 1
at distance 5878.42, camera `(5143, -1372, 2352)`, with every checked guest
instruction still original. The doorway image shows his visible head/arm
above the foreground hill. `Tomba2-objects-arena-final-after17.png` shows
the playable village's continuous sea/cloud view. The run reached 64,466,697
native dispatches with PGXP active, 7778 expanded packet frames, peak 85,852
bytes and 104 frames above the stock budget. No allocation/reset guard failed;
aspect queue highwater was `24/31/9` with zero rejects. Tomba's bone counts
stayed intact, and the game exited cleanly. The earlier native replay peaked
at 86,728 bytes across 107 over-stock frames. The original native DLL/ranges
were restored after these diagnostic runs; their temporary hook insertion is
not a production cache artifact.

The opening sequence needs both Zippo dialogue blocks, including the Journal
tutorial after the scripted house/pig camera tour. Twenty-three Circle presses
stop within that tour. Finish the second block until Zippo and the letterbox
bars disappear, then hold Right for four seconds and settle for one second
to reach the reviewed far doorway. The review helper
`advance_tomba2_intro_to_playable.py` provides a bounded Circle loop and checks
the known intro bars, rather than treating a scripted-camera capture as play.

## Primitive correction and backdrop seam

Native wide exact projection and near clipping are enabled. Exact NCLIP sites
`801466D8` and `8014682C` complement the existing packed-X triangle gates.
Five guarded BLTZ GTE-error sites preserve all relevant flags except horizontal
screen saturation in wide view. The shared emitter and interpreter use the
same policy and run the original signed predicate at 4:3. Both focused shared
codegen and interpreter checks passed.

The shared fast compositor placed a canonical 4:3 center over a stretched
phase/flat backdrop, producing mismatched water/cloud UV mapping at that
boundary. Same-scene fast-on/fast-off captures isolated the compositor. The
existing phase/flat backdrop opt-ins now require the full composite, as
explicit background tags already did. The actual playable-village capture
with fast-wide enabled shows continuous sea/clouds.

## Build and review

The primary USA target compiles PGXP, uses OpenGL native-wide presentation,
1080p internal resolution and the ENHANCED resident loading implementation.
Interpolation and generic host/CD-speed controls remain absent. Loader ABI
and source-input identity are unchanged by this repair.

Shared branch `fix/tomba2-packed-x-20261008`, pin
`7e7c94c24bb1b32fb539b6074cd008c3a04897d7`. Fresh OpenBIOS and SCPH1001
generation produced byte-identical normalized C before their emitter stamps
were refreshed; the strict stale-BIOS check remains enabled. The owner build
uses bundled OpenBIOS. Native input pairs are regenerated from the original
normalized disc inventory under the final configuration, rather than relabeling
old cache artifacts.

The final normal build and native cache passed the same intro-to-gameplay
route with no diagnostic code patches. The cache audit validates all 81
DLL/ranges pairs under `cg18_53aae7b5_gc90c4eebf_f2` against the original
normalized disc inputs. `receipts/Tomba2.production-visibility.json` records
old-man visibility 1 at distance 6104.22, active PGXP/native wide, 4153 expanded
packet frames, peak 85,852 bytes and 98 frames above the original packet budget.
The game reached frame 10318 and exited cleanly. Final village and doorway
captures show continuous sea/clouds and the visible doorway head/arm. The
receipt binds the executable and staged config by SHA-256; owner acceptance
is still pending.

The campaign review directory holds the exact evidence:
`receipts/Tomba2.objects-door-far-ab.json`, `...-door-far-revert.json`,
their RAM/image pairs, `Tomba2-objects-village3.png`,
`receipts/Tomba2.viewport-bios-refresh.json` and the final native cache audit.
These checks cover the reported village scene; they do not establish whole-map
or unseen-room coverage. Final owner checks: old man and pig while moving the
camera, continuous sea/clouds, then a room transition with intact music.

## Owner checkpoint and remaining visibility

The owner confirms the old man stays visible and requests committing the
current progress. Broader scene visibility is still incomplete: the far-right
pig and pole appear only after approaching, the sign left of the old man
fades in and out, and the tree behind the burning building disappears or loses
parts at different camera angles. Screenshot reference:
`Tomba2Recomp_dIuVWS7yKt.png`. These reports are tracked under
`beads-eio.2.14`; they are not covered by the old-man radius proof above.

The next investigation should identify each object's rejection or absence
from actual state. The owner proposes loading the whole current area without
subdivision selection and considering removal of camera-driven render culling
because the game uses unusual camera angles. Neither approach has been
implemented at this checkpoint. Rendering participation and asset residency
must be distinguished from gameplay spawning and actor lifetime when tracing
the remaining objects. Existing loader, renderer and old-man repairs are kept.
