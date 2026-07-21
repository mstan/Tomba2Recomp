# SCUS-94454 verified exact-entry fragments

The full-static Tomba 2 cache supplements the 53 play-free region shards with
12 isolated exact-entry fragments. These PCs were persisted live-session gaps:
the main shard covered their surrounding code but did not expose an exact native
entry. Each fragment is compiled from the play-free `0x80038000` capture, has its
own generated-C audit, and remains guarded by the normal per-function code CRC.

```text
0x8005597C
0x800561BC
0x8005628C
0x80056290
0x800564C4
0x80056594
0x80056598
0x800567C8
0x80056870
0x8005B174
0x8006C844
0x8006E0B4
```

Ten are direct branch continuations/shared epilogues. `0x8005B174` and
`0x8006C844` are indirect jump-table targets. They must not be promoted to
ordinary function roots or aliased to a prologue; both treatments would lose
the live register state expected at the exact PC.

Rebuild them by passing one `--force-interior <pc>` argument per address above
to `tools/compile_overlays.py` with the clean play-free capture and cache. The
compiler queues these explicit demands even when `executed_pcs` is empty and
publishes a fragment only when the requested entry itself has exactly one
runtime-representable guarded identity.

With these fragments, the coverage scoreboard reports 100.0% combined BIOS +
overlay code-range recall for the vault, current persisted live history, and the
pre-history persisted live gap set.

The clean 65-DLL cache (53 region shards + 12 fragments) was validated in the
static-only runtime on 2026-07-19. It crossed the formerly deterministic
sign/pit/ledge attract-demo freeze, returned to the title around frame 17,400,
and continued beyond frame 18,600 at 60 fps.
