# Mario's Hat continuation checkpoint

Last updated: 2026-10-24. Work solo. Main tracking: `beads-hal6`.

Worktree: `F:/Projects/psxrecomp/_wt-tomba2-marios-hat`, branch
`mod/marios-hat`; title HEAD `d0e729b`. Nested framework branch
`mod/tomba2-marios-hat`, snapshot fix `9e4cf574`. Both were clean on resume.
Read `MARIOS_HAT.md` for design, build commands, completed checks and open gates.

Recovered session: `01a16fa4-c12b-7a33-b63d-2acf27dfa8b1`. Its later tool
history completed a native save, but never completed the native reload check.
The final `continue` failed during remote context compaction.

Active private runtime: PID 64148, `build-mario/Tomba2Mario.exe`, hidden,
muted, TCP port 4597. Use `tools/mario_probe.py`; preserve desktop focus.
Private card/state directory is `build-mario/.research/test-saves`. Never
substitute owner cards. Slots 7/8 are quiet Tomba/Mario outside Fisherman's
house, 9 is voice, 10/11 are disposable interaction fixtures.

On recovery, card 1 contains one real native file `BASCUSP94454T2-00` in block
1 (state `0x51`). SHA-256:
`8b6be4e684ab36c7bf22331c3e770c1ae1340796d4b1aab021a6473b7c1c91c2`.
Card 2 remains empty, SHA-256:
`7706c7d43edaf8cb7618e574f03457105153e3bdc196db803a600ad96a8f58e8`.
The pre-save observation is `build-mario-assets/native-card-before.json`;
save-screen evidence is `native-card-saved.png`. Saved player position was
`[280312448, -68881920, 260046848]`, Mario requested/equipped, native costume 0,
only native items 0 and 10 owned. Continue checking native reload through the
game's actual Load menu, then compare ownership/progress and costume behavior.

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
the observations. Visual placement is still wrong (pig below Mario); inspect
the imported arm pose and native rendering transforms before implementing
overhead presentation. Live runtime currently follows that throw route.

Remaining gates include general hold/aim/throw and its presentation, native
death/continue, rewind/audio, matched built performance, adaptive
widescreen, picker/import UI and final packaging. Existing passing r14 and
weapon-family checks need not be repeated without a relevant change.

Preserve the successful AOT extraction/audit and private asset cache described
in `MARIOS_HAT.md`. No release/owner handoff has occurred. Unattended work stays
hidden/muted; an eventual owner handoff must be windowed with normal audio.
