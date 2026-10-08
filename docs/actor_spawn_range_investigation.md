# Tomba! 2 aspect-aware world participation

Status: latest-master correction validated on
`codex/tomba2-popin-fix` (2026-07-28). The root is based on `fa6a75d` and
the published framework commit is `a292a7c`, rebased onto framework master
`8cb378a`.

This document replaces the superseded experiment that forced thirteen
distance/cone comparisons. That experiment conflated several systems, changed
gameplay reach, admitted candidates without queue policy, and could reproduce
the black-screen/softlock signature. None of those thirteen
`widescreen.cull.keep` records is enabled.

## Desired behavior

At 16:9, 21:9, and during adaptive resizing, terrain and resident objects that
intersect the newly revealed horizontal field should participate before they
enter the old 4:3 field. Composite models must remain whole at the wide edges.
True 4:3 must execute the original comparisons exactly.

The implementation is deliberately not “infinite spawning.” It does not load
other rooms, expand actor pools, remove distance/near-plane checks, or activate
AI and combat outside their authored ranges.

## Fresh investigation

The investigation used the additive overlay-capture history plus the headless
Ghidra MCP program `psx/Tomba2`. Overlay addresses were accepted only when the
complete captured instruction matched; the composite RAM image was not treated
as proof for mutually exclusive overlay bodies.

The visible symptom separates into three relevant paths:

1. terrain-cell frustum selection;
2. engine model-list participation;
3. a lower-level per-model/per-child visibility bit.

Actor behavior, hit cones, room loading, and fixed queue capacity are separate
systems and remain unchanged.

### Terrain-cell selection

Six captured overlay constants are 12-bit angular half-extents used by the
terrain producers:

| address | expected instruction |
|---|---:|
| `8010E9A0` | `240301C7` |
| `8012EEB8` | `2403012C` |
| `8013F138` | `24020155` |
| `8013F190` | `240301C7` |
| `8013F224` | `2402013E` |
| `8013F244` | `240201C7` |

The runtime widens `tan(angle)` by the current horizontal reveal factor. It
does not add pixels to angle units. The complete instruction word guards every
site, so an unrelated overlay body at the same virtual address is untouched.

Measured values:

| client aspect | per-side extent (including 16px guard) | max vanilla | max widened |
|---|---:|---:|---:|
| 4:3 | 0 | 455 | 455 |
| 16:9 | 69 | 455 | 571 |
| 21:9 | 136 | 455 | 651 |

### Engine model-list participation

`FUN_8007712C` clears `model+1`, evaluates distance and a camera cone, marks an
accepted model visible, and inserts types 2/9, 4, and 5 into fixed scratch
queues. Their capacities are 24, 40, and 28 entries.

Only the six signed cosine-reject instructions are aspect-aware:

| address | expected instruction |
|---|---:|
| `800772D4` | `28620370` |
| `80077368` | `28620358` |
| `80077414` | `28620358` |
| `800774A8` | `28620370` |
| `8007753C` | `28620350` |
| `800775D0` | `28620368` |

Every vanilla acceptance is preserved. A vanilla rejection is reconsidered
with the horizontal cone scaled to the live aspect; the vertical cone and
distance checks stay vanilla.

The visible field has priority over the guard/hysteresis region. Guard and
hysteresis candidates are rejected when a relevant queue reaches
`capacity - 4`, reserving four slots for later visible candidates. The game’s
own capacity checks remain intact.

### Composite-child participation

The user’s new reproduction—an old man losing his body while his arms remain,
and a foreground barrel losing only some pieces—identified a lower-level
per-child gate.

Headless Ghidra shows `FUN_8002B278` computing:

```c
delta = object_position - camera_position;
distance = sqrt(delta.x² + delta.y² + delta.z²);
if (distance < 0x200)  reject;
if (distance >= 0x1C01) reject;
if (dot(camera_forward, delta) < distance * 0xD60) reject;
object[1] = 1;
```

The exact cone instruction is:

| address | expected instruction | threshold |
|---|---:|---:|
| `8002B368` | `0082202A` (`slt a0,a0,v0`) | `0x358` Q10 |

This site uses object register `s4` and camera-relative X/Z/Y registers
`s3/s2/s1`. It does not append to the three fixed model queues, so its
per-site `queue_guard` is false. Its near and far distance checks are not
changed.

This is the missing half-cull fix: individual children that vanilla accepts
remain accepted, while children in the aspect-derived horizontal fringe are
kept with the same vertical and distance envelope as 4:3.

## Runtime policy

- visible envelope: current client aspect, excluding the 16px guard;
- render/terrain guard: 16px outside the visible horizontal edge;
- explicit X/Z resident-object activation lead: another 256px beyond the live
  render margin (392px total per side at 21:9);
- deactivation hysteresis: another 24px outside the guard;
- true 4:3: zero margin and the original comparison result;
- native generated/GCC overlay code and the dirty interpreter implement the
  same activation-only transform;
- relevant aspect policy and every exact site contribute to overlay cache
  identity.

Changing window dimensions updates the envelope immediately. No recompilation
or cache rebuild is needed for aspect changes. Changing the configured
activation lead requires game/overlay regeneration because native code embeds
the bounded lead.

### Maximal resident activation without global terrain widening

The first maximal-view experiment raised the shared cull guard so the total
21:9 margin became 392px everywhere. That also increased the terrain angular
producer from the previously validated maximum of 651 to 807. The user
reported substantially worse terrain culling, consistent with over-admitting
cells into the fixed `0xFE` terrain lists.

That global experiment is rejected. `activation_guard_pixels` is now a
separate, bounded `[0, 256]` setting applied only to explicit `bias_sites` and
`range_sites`. At 21:9:

| path | margin / maximum |
|---|---:|
| visible reveal | 120px per side |
| render/terrain margin | 136px per side |
| explicit X/Z activation margin | 392px per side |
| terrain angular maximum | 651 |

This advances the known player-relative resident-object gate as far as the
supported configuration permits without changing terrain angles, projected
screen funnels, model cones, or their fixed-capacity queues. At true 4:3 both
the render margin and activation margin are exactly zero.

## Safety exclusions

- `FUN_80069B6C` is a behavior/teleport proximity trigger, not a pure
  visibility test. Its ground-plane X/Z pairs (`80069BA8`/`80069BB0` and
  `80069BCC`/`80069BD8`) remain aspect-widened because removing them provably
  drops nearby composite scenery and terrain in the starting-area demo. Those
  explicit X/Z pairs, plus opcode-guarded overlay aliases at
  `80110A08`/`80110A10`, receive the additional activation lead. Its vertical
  Y pair (`80069B84`/`80069B8C`) remains vanilla.
- The old far-distance sites (`8002B310`, `80077248`, `800772E4`,
  `80077380`, `80077390`, `80077424`, and `8007754C`) are not forced.
- Combat/attack angle windows in `FUN_8001FAE0` and overlay variants are not
  changed.
- Queue-capacity branches are not removed.
- Actor pools and unloaded rooms are not expanded.
- No address-only global comparison rewrite is used.

## Validation evidence

2026-07-28 maximal-view correction on latest master:

- root base `fa6a75d`; published framework `a292a7c` on master `8cb378a`;
  codegen hash `8d349ec4`; overlay config hash `6782d636`;
- guarded parser/code-generation tests pass, including range validation,
  overlay-cache identity, and isolated activation-lead emission;
- a fresh Release game regenerated 2,726 functions in six shards and built
  successfully after canonical OpenBIOS regeneration;
- the 21:9 attract run reached more than 42,000 guest frames with
  `x_margin = 136`, `activation_margin = 392`, terrain maximum 651, zero
  aspect-queue rejects, and queue high-water marks 16/25/6;
- 600 actual host-window captures at 0.2-second intervals covered the beach,
  furnace/chain, and mine-cart demos. The capture tool reported the window
  live across 423/599 consecutive pairs; loading/title intervals account for
  the static pairs;
- visual review retained continuous terrain, foreground machinery, background
  structures, signs, chests, props, and complete characters throughout the
  sampled attract sequences;
- the durable capture history records three executed overlay variants at the
  opcode-guarded `80110A08` activation site;
- a true-4:3 attract control reported both margins as zero. All 24,563
  aspect-cone calls and all 810 terrain-angle calls took their exact 4:3
  identity paths, with no wide keep/reject counters incremented.

Compiler/runtime validation:

- guarded recompiler parser and code-generation suite: all tests pass;
- aspect-cone math suite: all tests pass;
- game regeneration: 2,726 functions, six shards;
- overlay ABI: 21;
- codegen hash: `4cc1a66b`;
- corrected game-codegen config hash: `b7860373`;
- bundled OpenBIOS regenerated after the final emitter change;
- fresh-cache GCC autocompile completed 18 observed runs with zero process
  failures, zero failed shards, and a final one-built/28-safely-skipped shard
  result.

Execution-path validation:

- hybrid/native cache loaded the new identity and executed native overlays;
- interpreter control (`overlay_native_off`) produced zero native dispatches
  during the sample and 107,523 interpreted dispatches;
- in that interpreter sample, exact site `8002B368` ran 4,033 times and made
  122 additional wide-visible keeps;
- in the 21:9 starting-area pass, exact site `8002B368` ran 17,015 times,
  with 5,556 additional visible keeps, 34 guard keeps, six hysteresis keeps,
  and zero queue rejects;
- model-queue high-water marks were 16/25/6 against capacities 24/40/28, with
  zero aggregate queue rejects.

Historical 21:9 hybrid/native soak on the superseded `f8403a93`
configuration:

- 119 samples covered 595.859 seconds and frames 39,458 through 74,683;
- the guest frame counter advanced in every sample interval;
- median guest rate was 59.866 Hz and native dispatches increased by
  14,053,359 during the sampled intervals;
- candidate, range-index, lazy-manifest, and aspect-queue overflow counters
  remained zero;
- the three reported stale blocks were live overlay CRC transitions; every
  loaded library used the then-current `cg9_4cc1a66b_gcf8403a93` cache
  identity;
- the process remained responsive after the monitor exited, and the final
  host-window capture was 1280x548.

That soak establishes runtime stability but not visual correctness: its
screenshot schedule missed the later pond/house camera described in the
post-validation regression audit below.

Corrected X/Z-only `b7860373` 21:9 hybrid/native soak:

- 119 samples covered 597.579 seconds and frames 2,144 through 37,969;
- the guest frame counter advanced in every sample interval;
- median guest rate was 60.012 Hz;
- native dispatch increased by 8,577,609 during the sampled intervals;
- stale-block, candidate-overflow, range-index-overflow, lazy-manifest-
  overflow, and aspect-queue-reject counters remained zero;
- autocompile reached eight observed runs with zero process failures and zero
  failed shards; the latest result built 14 and safely skipped 17;
- queue high-water marks remained 16/25/6 against capacities 24/40/28;
- the process remained responsive at 60 FPS after the monitor exited.

True 4:3 identity:

- live resize selected mode 0 and `x_margin = 0`;
- during a five-second gameplay sample, all 111 calls to `8002B368`
  incremented only its `identity_43` counter;
- visible, guard, hysteresis, outside-reject, and queue-reject counters did not
  change.

Dynamic sizing:

- 4:3 -> mode 0, margin 0;
- 16:9 -> native-wide mode 2, margin 69;
- 21:9 -> native-wide mode 2, margin 136;
- every transition occurred in the same running process.

Visual evidence in the local validation build:

- `build-t2/wincap_exact_user_route/cap_12.bmp`: old man is a complete model
  at the doorway;
- `build-t2/wincap_exact_user_route/cap_18.bmp`: foreground barrels remain
  complete through the wide view;
- `build-t2/next_demo_check.bmp`: starting-area Evil Pig and distant objects
  participate in the initial 21:9 view;
- `build-t2/wincap_half_cull/cap_00.bmp` and `cap_15.bmp`: lava/mine terrain
  and models remain complete across the 21:9 frame;
- `build-t2/soak_21x9_final/final_host_window.bmp`: actual presented window
  after the sustained hybrid/native soak;
- window-capture motion checks remained live on every gameplay pair.

Post-validation regression audit:

- the initial screenshot set did not cover the later pond/house camera and was
  incorrectly treated as route-complete validation;
- removing every `FUN_80069B6C` pair reproduced a hard missing-terrain seam
  and absent nearby participants in that camera;
- restoring all three master pairs recovered the scene;
- a second generated-build A/B showed that the X/Z pairs alone recover the
  continuous terrain and nearby signs, ice blocks, pigs, cranes, barrels, and
  building pieces, so the Y pair remains excluded;
- evidence is in `build-t2/regression_live.bmp`,
  `build-t2/diagnostic_restore_check.bmp`, and
  `build-t2/diag_xz_target/cap_00.bmp` through `cap_19.bmp`.

The first clean-cache boot made before OpenBIOS regeneration correctly failed
validation: the build had warned of an emitter fingerprint mismatch and the
starvation dump stayed in OpenBIOS exception/SIO code (`BFC21xxx`) without
reaching the Tomba visibility sites. After canonical OpenBIOS regeneration,
the same empty-cache boot remained healthy and autocompiled successfully.

## Remaining scope

### October 8 village partial triangles

The owner's ladder/shop scene reproduces a separate failure after the earlier
actor participation work: the old man loses body triangles and the Evil Pig
loses faces in the widest supported view. Their projection flags are zero;
relaxing actor cones, adding vertical signatures or raising the global margin
does not repair it. Raising the margin instead overruns terrain visibility.

The loaded triangle funnel at `801465EC` shifts projected X into the high half
and compares it unsigned against `320 << 16`. The three comparisons are
`801466A0 = 0079C02B`, `801466A8 = 0199102B` (a delay slot), and
`801466B0 = 01F9C02B`. A private checkpoint A/B changing only these predicates
restores both complete actors. Vertical comparisons, NCLIP/backface decisions,
OT depth and capacity checks are left original.

The title declares these sites through the shared `widescreen.cull.packed_x`
configuration. Native code and dirty-RAM interpretation use the same signed
horizontal range expansion, guarded by complete original opcode. At 4:3 the
original unsigned comparison is retained exactly. The focused codegen/math
check covers all 65,536 signed X values and overlay opcode mismatch fallback.
Local before/after evidence is `Tomba2-partial-actor-baseline.png` and
`Tomba2-partial-actor-packed-x-ab.png` in the campaign review directory. Those
diagnostic captures are not shipped assets; rebuilt-candidate and owner review
remain distinct validation steps.

This solution covers candidates already resident in the loaded area. It does
not deliberately create actors from unloaded rooms or run distant AI early.
If a future scene proves that a visible enemy is not resident at all, that
scene needs separate allocation/lifecycle attribution before any pool change.
The known resident-area X/Z activation gate now uses the maximum supported
lead. Literal participation from unloaded rooms is intentionally not
implemented: it would require producer, consumer, storage, and overflow
analysis for each fixed pool.
