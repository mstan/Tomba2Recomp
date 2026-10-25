# Mario's Hat continuation checkpoint

Last updated: 2026-10-24. Work solo. Main tracking: `beads-hal6`.

Worktree: `F:/Projects/psxrecomp/_wt-tomba2-marios-hat`, branch
`mod/marios-hat`; native-save commit `edaef10`, overhead/facing commit `54054f6`.
See the latest title commit for the final culling/clearance fix. Nested framework branch
`mod/tomba2-marios-hat`, snapshot fix `9e4cf574`. Both were clean on resume.
Read `MARIOS_HAT.md` for design, build commands, completed checks and open gates.

## Current owner-feedback build: r23

**Latest: r24 production experiment is open, awaiting owner feedback.** R23 was
not cleared: the owner still saw slowdown and supplied
`Tomba2MarioCandidate_yuyoKmNbvm.png`, then identified the trigger as grabbing
the bucket on the fishing wire. The exact "carrying the Water" dialogue is now
reproduced and visually corrected in `cutscene-repro/bucket-fixed-r24-verified.png`.

The root cause was the shared heading conversion: native RotMatrix yaw maps
forward to **(cos h, -sin h)**, whereas Mario used (cos h, +sin h). Horizontal
0/2048 headings hid the error. R24 fixes the global Z sign, removes the ad hoc
costume-only half-turn and matches the fist's Z direction. The bucket cutscene
is mode 4/action 30/heading 1027; its captured native Q12 matrix +X column is
{-19,0,-4096}. Asset regression tests now use those independent native values.
Additional scripted yaw at +0x14e was investigated, but is zero in this actual
failure. No extra mode exception or native-matrix replacement was needed.

Repro: private slot 2 is a pre-bucket house approach at X=10500. Teleport the
player's X only to 10320, wait 10 frames, hold Cross 30 frames then release 70.
The actual native bucket contact grants item 40 and starts its dialogue; no
inventory or success flags are written. After settling, slot 0 holds the exact
dialogue. Native Tomba and pre-fix Mario references are retained. Slot 6 is a
busy gameplay fixture near X=11700; damage moves the player left, so assert
positive health/live gameplay during timing. Earlier `perf-busy-r23.json` and
`perf-busy-live-r23.json` reached death and are invalid performance comparisons.

Live owner observation in the busier area was 20.648 ms (~48 Hz), 1111 primitives,
190.6 batches. Native dispatch exceeds 99.9%; interpreted work is small. GPU timer
instrumentation issues hundreds of timestamp queries per busy frame. The R24
diagnostic build with `PSX_GL_PERF=0` measured **59.65 Hz** on the busy fixture
(`perf-no-queries-r24.json`), with health 2/live mode 1. An earlier cold R23 attempt
timed out and is inconclusive. Other owner game processes were also using the GPU;
they were left untouched. Do not claim the production runtime's FPS from this
diagnostic measurement.

The owner explicitly requested a production build with no diagnostics. It was
built Release with **PSX_DEBUG_TOOLS=OFF**, and build.ninja confirms
PSX_NO_DEBUG_TOOLS=1. `production-playtest/Tomba2MarioProd.exe` SHA-256 is
`ff9c897ca5fcf4eefae5fa92ae2524c0477e82398f31948257b65806230ac38b`.
The production directory has **123** original-byte-verified native pairs (81
offline + 42 additions). Runtime-only `0009A000_AB864F9F.dll` lacks an original
recipe guard match and was excluded from this staging, not relabelled as audited.
`PRODUCTION_BUILD.json` records flags, hashes, audit and experiment scope. The
existing TCP qualifier does not qualify a diagnostics-free binary.

Production owner PID **61636** is open in a normal window with normal audio,
same 1080p/5x/adaptive-wide profile and separately copied native cards. It has
no TCP listener. The production copy deliberately contains no cross-build debug
save states; use native Continue. Startup reached game main(). All hidden QA
processes have stopped. Do not automate against or close the owner production
window until finished. Retain prod-build for quick iteration during feedback;
it and task temp need cleanup when this handoff is finished. Preserve all cards.

The owner finished the r21 playtest; its window is closed. The active follow-up
addresses lag, backwards costume transformation, short punches, high/fast Mario
voices and surviving Tomba vocals. Work remains solo, with no import UI requested.

`build-mario/Tomba2MarioCandidate.exe` is the new build (SHA-256
`6eeb99f20e136399230f3473fabc2de75b2b8c7dd7f755ddc1bba3967d3310bd`).
The old `Tomba2Mario.exe` is preserved. The private playable runtime and evidence
are under `G:/Temp/Codex/psxrecomp/tomba2-mario-feedback-20261024/qa` and its
parent. Its copied cards are separate from the owner's earlier private cards;
never copy these fixtures back over the earlier cards.

Changes and completed checks:

- Apply a half-turn only to native mode 4/action 0x11 costume presentation.
  The unequip screenshot clearly shows Mario's face; equip has the same native
  state, but the captured frame is obscured by the transformation flare.
- Punch horizontal half/full widths are **actor +0x80/+0x82 = 64/128**;
  forward offset is 88. Old extent was 72+50=122; new extent is 88+64=152.
  **+0x76 is weapon-private state, not a hitbox radius.** The matched native pig
  route now hits at pretravel 8 where the old build misses; distant misses and
  ordinary native target handling remain intact.
- Apply Smash's actual FGM source pitch commands to the raw imported PCM:
  mostly -1199 cents, yielding 16009 Hz from the 32000 Hz synthesizer.
  Voice 430 uses 16280 Hz; 435 uses 16374 Hz. Audio capture correlation is 0.869
  against independently decoded/resampled source audio. Keep WAVs private.
- Replace the common Tomba vocal band (group 1, sound 0x1d..0x37), including
  death/hurt/effort cues; ordinary effects and area/NPC banks pass through.
  Remove duplicate action-byte voice triggers. A real native jump proves the
  replacement hook is exercised.
- R23 broad gameplay: **22/22**; real interaction suite: **30/30**. Asset/PCM
  tests and Python gate refusal tests pass. No unknown dispatches were recorded.

All 54 verified original-disc recipes (29 images) were rebuilt for the final
`cg18_05391a69_gcacf1fd28_f2` namespace: 81 audited pairs, 14619 manifest rows.
The offline inventory remains `build-mario-aot/disc-aot-5gv1i6ou/` and must be
preserved. Fourteen runtime page/orphan additions also match the original recipe
bytes; the offline auditor alone rejects their different page bases, so the
playtest tool separately checks their bounded function guards.

`tools/mario_playtest_gate.py` has prepare/qualify/launch actions. Qualification
requires the full audited shard set, a hidden runtime with isolated cards, native
dispatch coverage, no dispatch misses, >=57 VBlanks/s standing and moving, and
unchanged executable/config/tool/cache identities. Launch refuses missing, stale
or failed qualification. Do not weaken its threshold or rename cache namespaces.

The first cold movement run failed at 54.92 Hz and generated extra shards;
its failure is retained. After those original-byte-verified additions were
staged, a **fresh process restart** passed: Tomba/Mario standing 59.57/59.50 Hz,
movement 58.79 Hz, stable cache and no misses. This qualifies the measured village
route only; broader stages, rewind/audio and release packaging remain open.
The final gate also fixes a measurement defect: queueing 24 debug input steps
was previously included in movement wall time. It now queues first and measures
actual guest-frame progress between midpoint-timed frame-counter reads. Final
qualification passes at **59.99/60.03 Hz** standing and **58.63 Hz** moving.
Use `qa/PLAYTEST_QUALIFICATION.json` for this final gated handoff.

Retain the playable `qa` directory, useful reports/captures, original extraction
and private asset cache for feedback. The 54 compile logs are retained compactly;
compile-job scratch and duplicate staged caches were audited and removed
(about 1 GB). Preserve all native cards. The earlier r21 handoff below is historical.

## Latest continuation: basic checks and owner handoff

Recovered session `01a16ff8-ba29-74e3-b816-76eb9a46503d`, including the final
direction: **no import UI work; test basics such as death/continue and
widescreen, then hand off Mario gameplay for owner feedback**.

The previous session's r20 process (PID 34768) was still running hidden/muted.
Its on-disk death/Continue report passes six assertions without RAM writes;
the adaptive 4:3 -> 16:9 -> 21:9 -> 4:3 report passes eight assertions.
Composed captures were visually reviewed for real margin scenery. The r21
follow-up passes seven wide-inventory/current-build-load/card/dispatch checks
in `build-mario-assets/handoff-r21/preflight.json`. Both private cards still
match the hashes below. Binary SHA-256 remains
`e7b0af2dfeca4f77a5f6756a9e11513782f68198c1955bd3c03fe532eb5e3e8e`.

**Slot 9 is now the owner gameplay fixture**, Mario equipped outside the
Fisherman's house after Continue (native health 4). Slots 7/8 and 10/11 remain
the earlier private fixtures. The old voice test regenerates slot 9 when run;
do not run it against an owner playtest. Save-state slot IDs are 0 through 11.
The r20 hidden process has been stopped. The owner playtest was launched as
PID 60228 in a normal 1280x720 window with normal audio, the same private
cards/profile, and slot 9 loaded only after US MAIN.EXE was actually ready.
`handoff-r21/owner-launch.json` confirms equipped Mario, health 4, active host
audio and zero dispatch misses. Automated input has ended. Leave that process
for owner feedback; do not run additional gameplay checks against it.

No release archive is requested. Broader combat, rewind/audio, performance,
all-stage widescreen and final AOT/package qualification remain open; they do
not require redoing the completed basic checks before owner gameplay feedback.
The retained r21 reports, two composed screenshots, startup logs and slot 9
are useful handoff evidence. Clean them with the other private build/evidence
trees after feedback and implementation/release delivery; preserve native cards.

## Earlier r15-r18 checkpoint

Recovered session: `01a16fa4-c12b-7a33-b63d-2acf27dfa8b1`. Its later tool
history completed a native save, but never completed the native reload check.
The final `continue` failed during remote context compaction.

The final r18 private runtime (PID 49820) was stopped after validation.
Build: `build-mario/Tomba2Mario.exe`; TCP port 4597. Use the documented hidden,
muted launch in `MARIOS_HAT.md` for further unattended QA and preserve focus.
Private card/state directory is `build-mario/.research/test-saves`. Never
substitute owner cards. Slots 7/8 are quiet Tomba/Mario outside Fisherman's
house, 10/11 are disposable interaction fixtures. See above for current slot 9.

On recovery, card 1 contains one real native file `BASCUSP94454T2-00` in block
1 (state `0x51`). SHA-256:
`8b6be4e684ab36c7bf22331c3e770c1ae1340796d4b1aab021a6473b7c1c91c2`.
Card 2 remains empty, SHA-256:
`7706c7d43edaf8cb7618e574f03457105153e3bdc196db803a600ad96a8f58e8`.
The pre-save observation is `build-mario-assets/native-card-before.json`;
save-screen evidence is `native-card-saved.png`. Saved player position was
`[280312448, -68881920, 260046848]`, Mario requested/equipped, native costume 0,
only native items 0 and 10 owned. Native reload is now validated below.

The previous session tried Triangle to exit Save, which does not exit this
menu. Circle is Return; three bounded Circle taps returned to gameplay.
Native reload now passes, including a fresh process with generation 0 and
Mario not requested/equipped. See `native-reload-r15-fresh/report-audited.json`
and `native-reload-r15-final/report.json`. The initial fresh check's whole-card
equality failed because the native BIOS test writes system frame 63. SIO
transactions 38 and 91 exactly reconstruct both hashes; all other sectors
match. The initial failure is preserved and the audited report explains the
correction. `mario_probe.py` was fixed to read complete multiline responses.

The post-pig capture route now demonstrates holding, a Left turn (heading
0 -> 2048), and a native Cross throw/release through phases 3/4/6. It does
not establish every target's throw behavior. `capture-route-r15.json` records
the observations. The r16 build fixes overhead presentation using temporary
render-matrix translation and rigidly raised imported arms; gameplay coordinates
remain native. It also fixes the imported +Z forward-axis mapping to native
headings. Thirty interaction assertions and the broader 21-check smoke pass.
See `interactions-r16b-blackjack/`, `smoke-r16/`, and facing screenshots.
A history-read socket reset exposed EOF framing in the probe; complete-object
framing and two socket tests now pass.

The final r18 build retains F3DEX2 per-triangle cull modes and mirrors the
native capture attachment's full height above Mario's hands. Cap clearance was
visually checked in both directions. Both complete live suites pass in r18:
`smoke-r18/` (21) and `interactions-r18-blackjack/` (30). Asset/PCM tests and
the two probe socket tests pass. Final binary/source hashes and a source patch
are in `render-r18-identity.json` and `culling-clearance-r18.patch`.

Current quiet performance evidence is `perf-r17.json` and its reverse-order
pair: Tomba/Mario 57.88/58.18 VBlanks/s; reversed Mario/Tomba 59.42/59.57.
Mean host frames are 16.680–16.683 ms for all four cases. This is quiet 5x GL
costume parity; broader gameplay and a separate mod-disabled engine baseline
remain unqualified. Culling reduces emitted body triangles from 320 to ~157.
The r18-only change adjusts captured-model height and does not affect quiet
rendering. Earlier r16 slow/noisy results remain recorded in `MARIOS_HAT.md`.
Temporary offline pose probes/exports/contact images were audited and removed.

Remaining release gates include broader hold/aim/throw target coverage,
rewind/audio, broader built performance, wider stage coverage and final
packaging. Import UI was waived by the owner. Existing passing r14 and
weapon-family checks need not be repeated without a relevant change.

Preserve the successful AOT extraction/audit and private asset cache described
in `MARIOS_HAT.md`. No release has occurred. The owner gameplay handoff is
recorded above. Unattended work stays hidden/muted; owner playtests are
windowed with normal audio.
