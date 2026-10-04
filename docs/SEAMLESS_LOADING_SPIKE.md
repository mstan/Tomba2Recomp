# Tomba 2 resident-loading spike

Branch: `spike/tomba2-seamless-20261004`. Tracking: `beads-eio.2.8`.
This is an experimental, default-on USA mod, with a retail-loading opt-out.
It targets the original SCUS-94454 disc. The Italian catalog does not enable it.

The objective is to remove the game's loading screen between gameplay areas
without fast-forward, faster guest CD timing, audio discard, or gameplay
snapshots. Cutscenes retain their original sequencing and speed. Existing scene
fades are separate from the loading screen.

## Player setup

Players supply their original disc to a prebuilt binary. On first activation,
native code reads and verifies 34 non-streaming files, decodes 280 texture blocks,
and atomically writes a private resource pack. No Python, compiler, extraction
script, or runtime code generation is needed for this preparation. The native
executable and its audited overlay libraries remain developer-built artifacts.

The default cache directory is `%LOCALAPPDATA%/Tomba2Recomp/seamless` on Windows
and `$XDG_CACHE_HOME/Tomba2Recomp/seamless` (or `~/.cache/...`) elsewhere. The
filename includes a hash of the active mod-plan fingerprint. A changed asset
plan cannot silently reuse a stock cache. All source and decoded hashes are
checked when loading a pack. Corrupt or truncated packs are rebuilt from the
mounted disc; failure leaves the game using its original loader.

The resident set is 32,164,498 bytes: 23,480,320 bytes of sector-padded original
resources plus 8,684,178 decoded texture bytes. Music, speech and movies remain
on their existing streaming paths. Resource packs contain licensed game data
and must not be distributed or committed.

Disable **Resident Loading (Experimental)** in Mods to use retail loading.
Developer-only environment variables:

- `TOMBA2_SEAMLESS_CACHE`: cache directory override.
- `TOMBA2_SEAMLESS_TRACE=1`: resource/worker diagnostics; off by default.
- `TOMBA2_SEAMLESS_RETAIL=1`: prepare the pack but bypass the adapters, for A/B
  investigation. Normal opt-out is the Mods checkbox.

## What was assessed

| Approach | Result |
| --- | --- |
| Faster CD / turbo | Excluded: changes pacing and can disrupt audio. |
| Raw files in host RAM | Removes transport delay, but leaves decode, sample transfer and cooperative worker handoffs. |
| Decode textures before play | Implemented; verified against the original MIPS decoder for every catalog entry. |
| Resident adjacent areas | All immutable area resources fit in roughly 31 MiB, so the spike keeps the entire set resident. No neighbor prediction is needed. |
| Cache initialized destinations | Not used. Events, inventory, actors and destination initialization continue from current live game state. |
| Complete verified resource workers synchronously | Implemented for three original MAIN workers, with layout and call-context guards. This prevents their parent from entering the loading-screen loop. |
| General framework loader replacement | Insufficient by itself: the game's worker protocol, texture format and bank setup are game-specific. Only the mounted-disc file service is shared. |

## Original-disc evidence and adapter contract

The original disc SHA-256 is
`8e9568388155787384c3166a5934a495fbff3fa57154ab0489b0f214fe05a8ab`.
MAIN.EXE SHA-256 is
`cb580f5bd42895b1c452dffe53e1c8cb639a029e0afec194ae197fdfda9721ed`.
Addresses below describe original MIPS instructions, not inferred behavior
from generated C.

| Original routine/data | Role |
| --- | --- |
| `8001D940`, wrappers `8001DB8C` / `8001DC40` | Sector reader and scratchpad completion contract. |
| `80044BD4` | Starts worker 1 and renders the loading loop while completion is pending. |
| `80044F58` | IDX/IMG texture resource worker. |
| `8004514C` | Common resources and sound-bank worker. |
| `800452C0` | Area overlay, sound bank, texture, DAT and subarea resource worker. |
| `80044D8C` | Texture decoder; width-dependent byte backreferences. |
| `80051F14`, `80051F80`, `80051FB4` | Cooperative spawn, yield and termination. |
| `80099150`, called from `80096A00` | Ordinary `SsVabTransBody` sample DMA. Header allocation and voice metadata remain original code. |
| `800BE0F0` | Verified resource extent table. |
| `801FE070`, stack `801FF200` | Worker 1 TCB and original worker stack. |

The texture oracle executes the original decoder at `80044D8C` in Unicorn with
two different memory-fill patterns. It compares output bytes, return length and
surrounding canaries. Several original outputs extend a few bytes beyond the
texture rectangle size; the cache preserves the actual original output length.

The adapter copies only cataloged resource ranges to bounded guest RAM. Bulk
asset writes invalidate executable pages and overlay generations. It chains the
existing dispatch hook. It inlines only the three verified resource workers
from their known parent, with idle worker slots, original extent/stack layout,
and ordinary sound-transfer mode. Other worker functions, including the
prologue worker, retain their original scheduling. Unknown resource/texture
requests use the original routines. Modified code or loader layouts are outside
the synchronous adapter's verified scope; this is not broad asset-mod support.

The shared change is a backport of `psx_mod_read_disc_file`: it reads the mounted
disc with active sector patches while leaving guest CD state untouched. It does
not contain a Tomba-specific address, codec, timing policy or game-state rule.

## Validation and limits

- Native preparation integration test passes cold/warm loading, all 34 source
  hashes, all 280 decoded hashes, corruption repair, failed-repair cleanup,
  changed-mod-plan cache isolation, invalid bounds and XA exclusion.
- The independent original-MIPS oracle passes all 280 blocks under both fill
  patterns. The checked-in catalogs contain metadata only.
- Mod catalog tests cover USA default-on selection, retail opt-out, regional
  exclusion and existing packages.
- A live A00-to-A01 debug warp captured every guest display frame. Retail
  entered the loading screen; the resident resource worker finished in one
  guest frame and did not render it. Both paths retained 12 fully black guest
  frames around the transition. This is structural evidence, not clean
  performance/audio acceptance: see the test-setup findings below.
- A01 with the debug detours removed and boot handoff completed measured
  16.683 ms per presentation and about 59.7 guest frames/sec. A ten-second
  audio check had zero pump skips, mutes, underruns, host underruns or overflow
  drops. Earlier A00 gameplay also passed a 64-second idle audio check.
- A fresh launch through the original new-game prologue, using the newly
  audited native cache, measured 16.683 ms/presentation (16.888 ms maximum in
  the sample), with zero audio skips, mutes, underruns or overflow drops during
  a ten-second check. The scene then continued into A00 normally.

Representative natural routes, save loading, death/retry and later event
variants still need route coverage before a release claim. This branch is a
spike, not a claim that every transition in the game has been validated.

### Performance test setup findings

The existing debug-menu mod changes instructions at `80050CB0`, `80050CC0`,
`8007A904`, `8007A908` and `80108B60`. The original-disc native cache cannot run
those modified bodies; correct live-code guards force interpreter execution.
One test accumulated 580 million interpreted instructions at the modified
render loop. Leave that menu disabled for normal performance acceptance;
native coverage of its patched code is separate work. This is distinct from
the resident loader's resource adapters.

An early debug savestate restore can bypass the game-entry notification and
leave BIOS boot turbo enabled. The resulting run presents only one out of 30
guest frames and overflows audio, even though the ordinary turbo switches are
off. Tracked as `beads-eio.3.259`. Automation must wait for
`turbo_loads.game_started == 1` and `hle_dump.boot_turbo_active == 0` before
restoring, then verify all turbo gates again. Do not use affected runs as
performance or audio evidence. Savestates here are private test fixtures only.

The opening prologue's missing native coverage was tracked in `beads-eio.2.9`.
The spike's initial borrowed cache is not release provenance. A fresh build
from the original disc produced 78 audited native pairs / 14,613 manifest rows
for 53 game recipes with OpenBIOS; this excludes the profile's one retail-BIOS
resident helper, not any game image. Auditing proves source/guard provenance,
not full executable coverage or runtime correctness.
Cold-launch runtime validation subsequently confirmed normal prologue pacing.
The debug-menu native variant remains separate work (`beads-eio.2.10`).

## Reproduce asset checks

Build `tomba2_seamless_prepare_test` with the game CMake project, then run:

```text
tomba2_seamless_prepare_test ORIGINAL_DISC.bin NEW_EMPTY_CACHE_DIRECTORY
ctest --test-dir BUILD_DIRECTORY -R ^tomba2_preloaded_mods_test$ --output-on-failure
python tools/verify_seamless_assets.py ORIGINAL_DISC.bin --check
```

The developer oracle requires Python 3 and `unicorn`. It reads the owner-supplied
raw disc and the framework ISO reader, executes original MIPS, and compares both
metadata catalogs. Omit `--check` only when deliberately regenerating the
catalogs after verification. Player first-run preparation uses neither tool.
