# Resident terrain execution contract

`tomba2-resident-terrain` / `scus94454-area0-packet-v1` selects its implementation
at build time through `PSX_EXECUTION_PROFILE=ENHANCED|REFERENCE`. It is separate
from the resident asset loader. No implementation switch appears in the UI.

The USA area-zero hook at `8003D0BC` accepts the descriptor `800F2418` only after
23 original instruction/data words, the complete resident grid/mask layout,
selected-record membership and per-flip packet headroom pass validation. It
renders every nonempty resident cell: the original selector's unique prefix
first, then all remaining cells in grid order. Batches stop at that prefix
boundary and contain at most 254 records in a private descriptor. Unsupported
areas/code/layouts retain the original dispatcher.

Both implementations call the **same original guest routine** `801401B8`, with
the same descriptor and original `a1..a3`, returning at `8003D0FC`. They keep
every polygon packet, ordering-table link, GTE result and precision effect. The
shared call service restores caller GPR/PC/HI/LO, returns the last batch's `v0`,
and delays snapshots until the host continuation completes. The descriptor is
private to rendering; gameplay, visibility queues and asset ownership do not
observe its batch count. DMA submission remains in the original caller after
the renderer returns. No math replacement or polygon omission is introduced.

REFERENCE charges every original guest instruction as before. ENHANCED charges
the stock selected prefix normally and executes only the additional scenery
through `psx_mod_call_guest_uncharged`. That shared service retains callee
effects, restores the caller's timing deadlines/load pipeline and prevents
additional render work from advancing audio, timers, CD/DMA or VBlank. It does
not roll back packet/OT stores. A four-million-cycle budget thaws an unexpected
device wait and reports `tomba2.terrain.hle_budget_fallback`; normal completion
reports `tomba2.terrain.hle_batches`. This deliberately shortens the added
visual work's emulated duration. Shared CPU scaling is an independent title
configuration, not evidence that this family is performance-qualified.

The common grid traversal and exact original callee provide the LLE outcome
floor. Qualification additionally needs runnable REFERENCE and ENHANCED builds,
complete 343-cell draws, retained packet/queue bounds, measured useful scene
rate, screenshots and owner performance review. Continuous diagnostic GPU
readbacks must be disabled during timing; capture screenshots separately.
