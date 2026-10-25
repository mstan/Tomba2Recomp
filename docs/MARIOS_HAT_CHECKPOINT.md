# Mario's Hat continuation checkpoint

Last updated: 2026-10-24. Work solo. Main tracking: `beads-hal6`.

Worktree: `F:/Projects/psxrecomp/_wt-tomba2-marios-hat`, branch
`mod/marios-hat`; native-save commit `edaef10`, overhead/facing commit `54054f6`.
See the latest title commit for the final culling/clearance fix. Nested framework branch
`mod/tomba2-marios-hat`, snapshot fix `9e4cf574`. Both were clean on resume.
Read `MARIOS_HAT.md` for design, build commands, completed checks and open gates.

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
