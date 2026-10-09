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
`24/40/28`, with zero aspect-queue rejects. The standalone actual-plugin
contract check passed all 18 registrations, 4:3 identity, wide rejection
rescue, complete CPU register/PC preservation and failed-guard fallback.

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

The campaign review directory holds the exact evidence:
`receipts/Tomba2.objects-door-far-ab.json`, `...-door-far-revert.json`,
their RAM/image pairs, `Tomba2-objects-village3.png`,
`receipts/Tomba2.viewport-bios-refresh.json` and the final native cache audit.
These checks cover the reported village scene; they do not establish whole-map
or unseen-room coverage. Final owner checks: old man and pig while moving the
camera, continuous sea/clouds, then a room transition with intact music.
