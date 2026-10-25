# Mario's Hat development

Status: substantial implementation in progress, **not release ready**.
Tracking: `beads-hal6` (title) and `beads-qbbc` (framework PCM callback).
The current target is US SCUS-94454, Windows, OpenGL. Other renderers and
regions are not qualified. No Nintendo ROM, extracted asset, sampled animation
or PCM data belongs in source control or a distributable.

## Design and controls

The optional mod adds a permanent virtual inventory item, Mario's Hat. Equip
or remove it through the ordinary inventory using Tomba's native costume
transaction and transformation animation. The item remains available without
setting an ownership byte in the native save structure. Other costumes are
exclusive with Mario and must restore Tomba.

The approved v1 uses Smash Bros. 64 Mario: movement, one extra airborne jump,
fist-range attacks retaining the equipped weapon's damage/target rules,
overhead capture, and native hold/aim/throw. Native paths, collision,
attachments, shadow and progression rules remain authoritative. Explicit
suit/event gates remain. Fireball, Super Jump Punch and tornado are deferred.
Controls remain Triangle for inventory, Cross for jump/confirmation, Circle
for attack/talk and native controls for carrying/aiming/throwing. These design
statements are not a claim that every interaction below has passed QA.

## Local import

The bundled US manifest is
`mods/preloaded/packages/tomba2.character.mario-hat/0.0.1/manifest.toml`.
Version 0.0.1 is Experimental and default off. The existing format-9
native resource picker/import pipeline uses `prepare = "tomba2.mario.import"`.
The ROM resource is input-only, shared as `n64.smash64.us`; the prepared asset
resource is hidden. The US runtime stages this manifest into its normal Mods
catalog, including release builds. The Italian catalog does not include it.

The importer accepts canonical US 1.0 data in `.z64`, `.v64` or `.n64` byte
orders, normalizes it and verifies size and SHA-256 before extraction:

The ROM is mandatory whenever the mod runs, including when imported assets
already exist. The trusted preparer validates the selected ROM before checking
the asset cache. Keep that ROM accessible at the selected path; a cache alone
does not satisfy the requirement. Missing selection, an unavailable file and
a correctly sized N64 file with the wrong SHA-256 were each rejected by the
production executable (exit 1), with the valid imported cache still available.
The isolated rejection evidence is `rom-requirement-r28/report.json` under the
retained feedback task directory. No owner gameplay was relaunched.

| Identity | Value |
| --- | --- |
| Canonical ROM bytes | 16,777,216 |
| Canonical ROM SHA-256 | `15592e79d3c5295cef4371d4992f0bd25bec2102fc29644c93e682f7ea99ef3d` |
| Cache magic / revision | `MARIOR02` / `r2` |
| Cache bytes | 678,804 |
| Cache SHA-256 | `1cbe9ce74e63464da8582d14dea5f8ba94e30a8c0258f03fee80f9abb3ace588` |

The cache contains 25 joints, 320 triangles, 18 textures, 34 sampled motions
(1,424 frames) and 13 locally decoded Mario voices. It is published under
`%LOCALAPPDATA%/psxrecomp/imports/tomba2-mario/r2/`. It is private owner-derived
data, not a redistributable build input. Decoder metadata is derived from
`ssb-decomp-re` commit `572a747d8171137718fad472c5d7473dc7309ffd`;
the ignored research clone is not a vendored asset dependency.

For standalone importer/decoder checks, configure with the native MinGW
compiler and Ninja, then build the small test project:

```powershell
& 'C:/msys64/mingw64/bin/cmake.exe' -S tests/mario -B build-mario-assets -G Ninja `
  -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe `
  -DCMAKE_MAKE_PROGRAM=C:/msys64/mingw64/bin/ninja.exe `
  -DMARIO_TEST_CACHE="<private r2 cache path>"
& 'C:/msys64/mingw64/bin/cmake.exe' --build build-mario-assets --parallel 2
& 'C:/msys64/mingw64/bin/ctest.exe' --test-dir build-mario-assets --output-on-failure
& './build-mario-assets/mario-import.exe' '<owner ROM path>' '<new private cache path>'
```

Without an owner cache, the asset test runs malformed-input checks then reports
skipped (77); the separate deterministic PCM test requires no ROM. The native
picker/import UI has not been validated; importer success alone does not
establish that UI workflow. The owner explicitly waived import UI work for
the current gameplay handoff on 2026-10-24.

## Runtime implementation

`src/mods/tomba2_mario_plugin.cpp` redirects guarded item metadata/icon table
accesses into guest allocations and appends item ID 256. Inventory lookup
returns a full-width virtual ID; it never indexes the 256 native ownership
bytes with that ID. The native costume byte remains ordinary costume zero.
The mod's requested/equipped state lives in guest memory.

The imported body uses sampled source poses, mod texture banks and GT3 packets.
Its 18 materials share a 256x64 immutable atlas with replicated borders;
material origins change packet UVs without changing imported texels or OT order.
Original actor logic, attachments and shadow continue to run. Weapon-family
actors provide short-lived fist collision windows; their original family
handlers perform retirement/pool accounting. Real damage and weapon-specific
target rules still require meaningful controlled interaction fixtures.

Framework `psx_mod_register_pcm_plugin` callbacks mix after SPU/shadow synthesis
and before audio capture or host muting. The integer voice resampler has an
explicit source cursor and fractional phase in guest memory. Loading a state
rebinds textures without clearing valid restored metadata or voice state.
Current-build save/load, rewind and rollback must remain correct. States from
other builds are disposable; native memory-card progress is a separate contract.

## Build and live smoke tooling

Use the isolated title worktree and its nested framework. Preserve the asset
junction `tomba2`; do not recursively remove or replace it. Check drive space
and I/O before substantial generation/builds. Use native executables directly
from PowerShell with profile/login behavior disabled.

Regenerate after hook-list changes; never edit generated C:

```powershell
& './psxrecomp-v4/recompiler/build/psxrecomp-game.exe' --config game.toml
& 'C:/msys64/mingw64/bin/cmake.exe' --build build-mario --target psx-runtime --parallel 2
```

Build only `psx-runtime` in this worktree: the default all target also requests
Italian generated code that is absent. Development overrides name the binary
`Tomba2Mario.exe`, with debug tools on and port 4597. Private launch arguments:

```text
--hidden-window --no-launcher --game .research/game-test.toml
--bios build-mario/bios/openbios.bin --debug-port 4597
--memcard-dir .research/test-saves
```

The private cards/states resolve under `build-mario/.research/test-saves`.
Use process-local `SDL_AUDIO_DRIVER=dummy` for unattended work and prepend the
native MinGW bin directory to process-local PATH. TCP screenshots, RAM reads
and input routes preserve desktop focus. Owner QA is a separate explicit
handoff in a normal window with normal audio.

`tools/mario_probe.py` exposes bounded live RAM observations, input routes,
state completion receipts and screenshots. `Probe.actors()` walks the native
active lists, not a list of proven enemies. RAM reads are live observations,
not atomic snapshots. Byte writes can race native physics; a position write
that is immediately corrected by the native path resolver is not a valid
interaction fixture. The attempted debug placement hook was discarded because
the native player path context still corrected the requested location.

```powershell
python tools/mario_smoke.py --output build-mario-assets/smoke-current
```

Run only against an isolated private-card runtime. Slot 7 must be quiet,
grounded Tomba gameplay near the Fisherman's house, owning exactly two native
equipment entries. The script fails if this layout differs. It writes a Mario
fixture to slot 8 and a mid-voice fixture to slot 9; slots are configurable.
Button durations use guest VBlanks through `input_route_*`, never pause/step.
The report distinguishes passed assertions from unverified contracts.
The helper waits for actual game entry and the US MAIN.EXE function identity
before the first restore. Restoring too early during boot left host title/boot
context active in a test attempt. Inventory checks also verify that the native
menu opened and that the virtual item is selected before confirming it.

## Validation and remaining gates

The r11 message-queue build passed 21 live assertions: native inventory transaction,
equip/unequip, first jump, one extra airborne jump, rejected third press,
landing rearm and a new extra jump on the next cycle, current-build load with
an imported voice active, cursor completion, and unchanged native ownership
bytes. The longer native extra-jump voice provides the mid-voice save fixture;
the short first-jump voice can finish before asynchronous observations return.
The separate asset and PCM tests passed, including preservation of every atlas
texel and edge borders. Five framework tests passed again:
`mod_runtime_test`, `mod_packages_test`,
`mod_texture_banks_test`, `mod_memory_snapshot_test`,
`spu_end_without_repeat_test`.

Visual evidence `build-mario-assets/ui-r6-equip-inventory.png` and
`ui-r6-equip-toast.png` verifies the cap is inside its native icon slot, the
ordinary Blackjack row retains its name, and the confirmation names Mario's
Hat. `smoke-r11b/` retains current screenshots and the assertion report.
`atlas-r10-front.png` verifies face/cap texture mapping after atlas packing.

`tools/mario_suit_smoke.py --item 15 --output <private evidence directory>`
checks exclusivity through real native menu transactions in both directions.
Squirrel Clothes (15), Bird Clothes (14), and the first Evil Pig robe (18)
passed eight assertions each in r11, including confirmation consumption and
restoration of the original ownership bytes. Acquisition is deliberately
bypassed by granting only the chosen native item and its inventory sort byte
in private RAM; these checks do not establish acquisition or event gates.
Reports and screenshots are in `suit-r11-squirrel/`, `suit-r11-bird/`, and
`suit-r11-robe/` under `build-mario-assets`.

`tools/mario_interaction_smoke.py --output <private evidence directory>` walks
from the Mario slot-8 fixture to the first post pig. It compares the same
approach with and without an attack, checks a distant miss, observes an actual
native capture link, and saves/loads that capture. Actor observations come
from the recorded frame ring. It writes disposable slots 10/11 by default and
restores the input fixture and previous snapshot-region configuration. No
actor position, health, capture or success flag is changed. `--weapon 3` and
`--weapon 6` grant Boomerang and Hammer ownership/order in private RAM and use
the native inventory to equip them; acquisition is bypassed and the original
ownership is restored. These are ordinary-pig interaction checks, not proof
of damage amounts or weapon-specific target gates.

The r12 test exposed a real Hammer defect: its fist was retired after four
total handler ticks, while the native swing was still winding up in phase 2.
The r13 implementation counts four active ticks after the Hammer reaches its
damaging phase, bounds interrupted lifetimes, and clears its native terrain
impact latch on forced retirement. The Hammer now passes the real hit/miss,
balanced retirement, capture and capture-state restore checks (27 assertions,
`interactions-r13-hammer/`).
Blackjack and Boomerang passed the same rebuilt-runtime comparison (23 and
27 assertions respectively), including out-of-range misses and restored
ownership. Evidence is in `interactions-r13-blackjack/` and
`interactions-r13-boomerang/`. The broader r13 smoke rerun exposed a pending
save that completed after its short jump voice ended; that snapshot-timing
investigation is tracked separately as `beads-qrwo`. Do not claim a fresh
mid-voice save/load pass for r13 from the earlier r11 evidence.

The r14 framework fix (`9e4cf574`, `beads-qrwo`) polls staged user snapshots
before the amortized interrupt fast returns, retaining the exception, native
resume-PC, host-adapter and mod-callback safety gates. Save-status receipts
now report poll counts and deferral reasons. The revised smoke tool submits
the save when the real extra-jump voice starts, then waits for actual restored
frames before inspecting history. All 21 checks passed in `smoke-r14-final/`:
the save completed on its first poll and recorded frames restored voice 436
with its cursor and fractional phase. Six focused framework tests passed,
including the extended host-snapshot boundary test.

A matched quiet-scene check sustained approximately 60 VBlanks/s before and
after this framework fix (16.681/16.689 ms mean host frames). Long-frame
outliers also occurred with the unchanged framework in the exact same saved
scene (187.727 ms baseline maximum versus 172.216 ms candidate); they are not
established as an introduced regression. These capped measurements qualify
this narrow comparison, not the full mod performance gate. Binary hashes,
source diff and measurements remain under `build-mario-assets/snapshot-poll-*`.

`tools/mario_voice_smoke.py` (NumPy required) stimulates guest voice 437 and
correlates the real post-SPU WAV with independently resampled owner PCM. The
r10 capture in `voice-r10/` passed at normalized correlation 0.814 and the
voice cursor completed. This is an audio integration check, not proof of the
gameplay event that normally triggers the voice. Use only private output and
cache paths; the WAV contains owner-derived sound and must not ship.

`tools/mario_perf.py` compares quiet Tomba and Mario fixtures in the same
binary/configuration/scene. The mod is loaded in both cases, so this does not
qualify a separate pre-framework or mod-disabled baseline. In the 5x hidden GL
measurements, the original material banks produced about 101 batches/present
and 2.13 ms flush time as Mario. The atlas reduced these to 12.3 batches and
0.375 ms, close to the paired Tomba's 10.5 batches / 0.373 ms. Throughput was
noisy: the earlier pair measured 47.16/37.05 guest VBlanks/s (Tomba/Mario), the
reverse-order atlas pair 33.36/32.23. Do not claim that the final frame-time
budget or cross-build performance gate has passed from these variable runs.

The r15 native-card check loaded the file saved while Mario was equipped,
first in the same process and then from a fresh process with no savestate
transactions. The fresh load restored ordinary Tomba at the saved sign with
the expected native ownership bytes and responsive movement. Mario selection
is session state and is not embedded in the native save. The unchanged card 2
and all card-1 progress sectors were verified. The only first-load card change
was system write-test frame 63: native SIO writes 38 and 91 reconstruct the
before/after hashes exactly. Initial whole-card equality was too strict;
`native-reload-r15-fresh/report-audited.json` retains that correction and its
proof, while preserving the initial failed report. The revised
`tools/mario_native_reload_smoke.py` passed 15 assertions in
`native-reload-r15-final/`. It uses only native menus, with no RAM writes or
snapshot loads. `mario_probe.py` now accepts complete multiline JSON responses;
the card transaction diagnostic pretty-prints its arrays across lines.

An r15 recorded capture route held the post pig, turned from native heading
0 to 2048 with Left, and used Cross to execute phases 3/4/6 and release the
pig through its native handler. This is still the special first-post pig;
its visible placement was below Mario, so that build did not implement
overhead presentation correctly. Evidence: `capture-route-r15.json` and
`capture-{hold,aim-left,throw}-r15.png` under `build-mario-assets`.

The r16 fix raises only the imported arm render matrices and translates the
captured actor's native model matrices during its draw call, then restores
every matrix. Native actor coordinates, links, collision and release remain
unchanged. The source model faces +Z, not the previously assumed +X; corrected
yaw now follows native headings. Asset checks cover all four quarter turns,
unchanged non-arm bones and rigid arm lengths. `interactions-r16b-blackjack/`
passes 30 assertions, including hold, both aim directions, unchanged native
captured coordinates, active overhead drawing and actual throw/release. The
broader 21-assertion smoke also passes in `smoke-r16/`. Screenshots
`facing-{left,right}-r16.png` and the interaction captures show the correction.

Probe framing now stops at a complete JSON object instead of requiring EOF:
the latter raced Windows socket reset during a history read. Two socket tests
in `tests/mario/test_probe.py` exercise fragmented multiline replies (including
quoted braces/escapes) and failure responses while the server keeps its socket
open. Both pass; live interaction/history and 2 MiB RAM reads also pass.

The r16 matched quiet-scene runs remain below the desired runtime target:
forward order Tomba/Mario 59.49/55.65 VBlanks/s, reverse order Mario/Tomba
47.81/57.29. Other compiler processes heavily used the host (one `cl` process
reported 645% CPU), and long-frame outliers varied, so these are not a clean
final performance qualification. Mario emitted 320 body triangles without
culling; the original Smash fighter display state uses `G_CULL_BACK`. Check
source geometry modes and built behavior before changing this policy.

The r17 decoder now retains F3DEX2 geometry-mode culling per triangle. The
normal fighter draw in the referenced Smash source explicitly enables backface
culling; the imported model's geometry-mode commands can retain two-sided,
front-cull or both-cull behavior. The native renderer applies that policy after
near clipping and projection. In the quiet scene this reduces emitted body
triangles from 320 to about 157 without losing the visible face/cap/gloves.
The front/back/two-sided mode checks and owner asset/PCM checks pass.

Both matched r17 orders now show quiet-scene parity: forward Tomba/Mario
57.88/58.18 VBlanks/s and reverse Mario/Tomba 59.42/59.57. Mean measured host
frames were 16.680–16.683 ms in all four cases, with 20.235–22.375 ms maximums.
Reports are `perf-r17{,-reverse}.json`; source hashes, executable identity and
the source patch are retained alongside them. This qualifies this quiet 5x GL
costume comparison, not all gameplay or a separate mod-disabled engine baseline.

The r18 draw adjustment mirrors the native capture attachment's full
`80 + target height` offset above the imported hands. This clears the pig from
Mario's cap while turning. Both directions were visually inspected in
`interactions-r18-blackjack/`. All 30 interaction assertions and all 21 broader
smoke assertions pass in r18, including current-build captured-state restore,
actual native hold/aim/throw and the active voice restore. No dispatch misses
were recorded, and both private card files still match the post-save hashes.
`render-r18-identity.json` records the tested binary/source hashes. The hidden,
muted runtime was stopped after testing; its build, fixtures and evidence remain.

The r20 basic checks survived the failed session and were recovered from
`build-mario-assets/basic-r20/`. Native contact damage reached zero health,
the death/Continue screen appeared, and Cross returned to responsive gameplay
with positive native health, Mario still equipped, a rearmed extra jump and
unchanged inventory. All six Continue assertions passed, with no RAM writes
or dispatch misses. Both private card hashes match the validated r18 values.

The enabled adaptive widescreen profile followed 960x720, 1280x720,
1680x720 and 960x720 windows (eight assertions). Composed `present_shot`
captures show real additional terrain/scenery at 16:9 and 21:9 and the return
to 4:3. These are introductory-village checks, not an all-stage widescreen
audit. The r21 follow-up passes seven checks: wide inventory opens/closes,
Mario remains equipped, current-build save/load restores health, inventory
and rearmed extra jump, both cards are unchanged, and dispatch misses remain
zero. Evidence: `build-mario-assets/handoff-r21/preflight.json` and its two
composed screenshots. An initial probe requested invalid slot 12 and aborted;
the corrected report uses slot 9 (valid slots are 0 through 11).

Slot 9 is now the post-Continue Mario gameplay handoff fixture outside the
Fisherman's house, replacing the disposable r20 widescreen fixture. The
earlier voice-restore reports remain evidence; rerunning the voice smoke
regenerates its fixture. The owner will assess actual Mario gameplay and give
feedback. No new build or engine change was needed for these checks.

Still unverified: damage amounts and weapon-specific target requirements,
broader capture/aim/throw target coverage, rewind/audio replay, broader runtime
performance and widescreen coverage, release defaults and packaging. Import UI
is waived for this handoff. A post approach
caused native Tomba and Mario to enter the same blocked/hurt states; it is not
proof of a working hit or capture. The later controlled interaction fixture
above provides separate hit/stun and capture evidence. Relocated actor `0x800fc8d8` was visually
identified as an NPC, so its counters/memory changes are not combat evidence.

The expensive original-disc AOT pipeline completed 29 images / 54 recipes and
staged 81 audited native pairs in `build-mario-aot/staged-audited`, with
`AOT_CACHE_AUDIT.json`. Preserve this evidence and the successful extraction
`build-mario-aot/disc-aot-5gv1i6ou`. The toast fixes changed the configured
function hook from `0x80078ca8` through `0x8004ed0c` to `0x8004fa38`; the resulting native cache
namespace differs from that earlier audit. Final staging must use an audit
matching the final hook/codegen configuration. Do not blindly copy or rename
the old cache namespace, or restart every recipe before checking which inputs
and artifacts can be reused by the supported pipeline.

These tests qualify the basic development-build handoff, not feature
completion or a release archive. A future archive must exclude ROMs/imported caches and both
`psx.enhancement.cd-speed` and `psx.enhancement.fast-loading` packages. Keep the
optional character feature default off. Keep both Beads issues in progress
until their requested outcomes are validated.

Cleanup retained the successful AOT extraction/audit and private r2 cache;
two failed extraction directories containing only their failure logs and the
obsolete local-app-data r1 cache were audited and removed. Builds, current
states, useful screenshots/WAVs and performance reports remain private task
evidence and need cleanup after the implementation/release handoff.
The temporary r15–r18 offline pose-probe source, executable, raw exported mesh/
pose JSON and contact images were audited and removed after the presentation
checks; no new bulk scratch remains outside the retained build/evidence trees.

## Owner feedback and gated r23 playtests

R23 corrects costume-transformation facing, extends the fist's forward native
collision extent from 122 to 152 units, and restores the imported voices' source
pitch and duration. Smash FGM 429..441 selects tables 296..308; their first pitch
commands lower the raw waveforms by approximately an octave. The previous mixer
ignored that articulation. Common Tomba vocal events now choose Mario cues at
the native sound wrapper instead of adding a second voice from action changes.
See `MARIOS_HAT_CHECKPOINT.md` for exact fields, rates and current evidence paths.

R23 passes 22 broad gameplay checks, 30 real interaction checks, native asset/PCM
tests and Python gate tests. A matched input route demonstrates a newly reachable
pig hit while distant punches still miss. Resolve that target by native handler
and authored coordinates: actor-pool addresses change after Continue/reloads.

The new playtest tool prevents a repeat of a stale/incomplete shard handoff:

1. `prepare` rebuilds the verified original-disc recipes with the supported
   framework pipeline and stages its audit with the matching cache namespace.
2. `qualify` verifies every audited artifact, proves runtime page/orphan additions
   against bounded original-disc bytes, binds the debug port to the actual hidden
   executable, and measures standing and movement with private fixtures. It
   requires >=57 VBlanks/s, >99% native dispatch, no misses, live Mario gameplay
   and no artifact changes during the measurement.
3. `launch` accepts only a passing report for the exact executable, configuration,
   tool and cache. It opens a normal window with normal audio and private cards.

Use Python 3.11+ and native Windows GCC. Example qualification/handoff after a
hidden, muted QA process is running on 4598 with current-build slots 7/8/10:

```powershell
$py = 'C:\Users\Matthew\AppData\Local\Programs\Python\Python312\python.exe'
$qa = 'G:\Temp\Codex\psxrecomp\tomba2-mario-feedback-20261024\qa'
& $py tools/mario_playtest_gate.py qualify --exe "$qa\Tomba2MarioCandidate.exe" `
  --inventory build-mario-aot/disc-aot-5gv1i6ou/runtime-input-inventory.json `
  --qualification "$qa\PLAYTEST_QUALIFICATION.json" --port 4598
# Stop that automated process before the owner-facing launch:
& $py tools/mario_playtest_gate.py launch --exe "$qa\Tomba2MarioCandidate.exe" `
  --qualification "$qa\PLAYTEST_QUALIFICATION.json" --cards "$qa\cards" --port 4598
```

The first cold movement qualification correctly failed (54.92 Hz plus new cache
files). With the additions retained and independently checked, a fresh process
passed at 59.57/59.50 Hz standing as Tomba/Mario and 58.79 Hz moving. The cache
remained stable and native dispatch exceeded 99.9%. These are village-route
measurements at the current profile; they do not establish every area's
performance or release readiness. Never rename an old namespace to satisfy the
gate. A new binary/configuration/cache requires new qualification.

The final timing code queues debug inputs before measuring and uses actual
guest-frame progress between midpoint-timed counter reads. Earlier movement
rates included the setup time for 24 input RPCs and understated throughput.
The corrected final gate passes at **59.99/60.03 Hz** standing as Tomba/Mario
and **58.63 Hz** moving; its exact-identity report is
`qa/PLAYTEST_QUALIFICATION.json`. The 54 compile logs are retained, while roughly
1 GB of disposable compiler jobs and duplicate caches has been removed.

## R24: bucket dialogue and production comparison

Owner feedback exposed that r23's costume-only facing exception was too narrow.
The native yaw matrix faces (cos h, **-sin h**) in X/Z; the imported conversion
used +sin h. R24 corrects this globally, removes the special-case half-turn,
and corrects the fist's Z direction. The exact bucket-on-fishing-wire dialogue
was reproduced through real native pickup contact and visually compared with
Tomba. Tests retain its independently observed native yaw/matrix values.

Busy-area slowdown also survives r23's narrow village gate. R24 with GPU timer
queries disabled measured 59.65 Hz in the busy fixture; the owner requested the
stronger experiment of compiling diagnostics out entirely. That Release build
uses `-DPSX_DEBUG_TOOLS=OFF`, retains the same 1080p/5x/adaptive-wide profile, and
stages 123 native pairs whose guards match original recipe bytes. One unproven
runtime-only pair was excluded. This is an owner-requested production trial,
with its hashes/flags in `production-playtest/PRODUCTION_BUILD.json`; no TCP
performance qualification is claimed for it. Native cards are separately copied,
and debug save states are not carried across. See the checkpoint for the open
owner process, exact repro and retained evidence. Production build scratch is
retained for ongoing feedback and must be cleaned when no longer needed.

## US reverse-engineering notes

Addresses below were checked against live US SCUS-94454 instructions/data.
Overlay address identity must be requalified for another stage/revision.

| Address / field | Meaning |
| --- | --- |
| `0x800e7e80` | Player actor; `+4` mode, `+5/+6` action/phase, `+0x2c/+0x30/+0x34` Q16 world position |
| Player `+0x145` | Native air phase; zero after landing. `+0x29` is not a reliable standing/grounded flag |
| `0x800bfab4`, `0x800bfbb4` | Native ownership bytes and inventory ordering |
| `0x800a2be8`, `0x800a24f4` | Native item metadata and icon-index tables |
| `0x800376ec` | Item-name renderer; menu/item/column/y in a0..a3 |
| `0x8007e938` | Sprite wrapper: a0 is a sprite bank, a1/a2 anchor x/y, a3 UI layer |
| `rw(0x800ed8c8) + layer*4` | Actual UI ordering-table link |
| `0x8004ed0c`, caller `0x8004d6e4` | Ordinary equipped-message producer and costume wrapper return |
| `0x8004eddc`, caller `0x800343fc` | Two-item remove/equip message when replacing a native suit |
| `0x8004fa38` | Native message queue; a1 = -1 accepts already encoded text |
| `0x8004ead0` | Encodes an item name: equipment colour `0xf4`, character minus 32, space/newline `0xfb`, ordinary colour `0xf0`, terminator `0xff` |
| `0x800fb168`, `0x800f2624`, `0x800f2738` | Three active-list heads. Node `+0x20` is previous, `+0x24` next, `+0x1c` handler. The third list can be empty during gameplay |

The inventory cap is an imported 8x4 sprite drawn at 2x, centred around the
native icon anchor. The old broad text-render replacement could rename an
ordinary Blackjack row while a timeout was active. The replacement handles
only a pending hat transaction at the message queue, checking the encoded
item/status tokens and saved costume caller addresses on the native stack.
Both ordinary equip/remove and replacement of a native suit use the hat name;
weapon and shoe confirmations and ordinary inventory labels remain native.
