# AOT static-coverage recall — SCUS-94454

_How much of the played reference set did the play-free static extractor reproduce, and how much lies in compiled static code?_

- Static shard cache: `build-aot/cache-purestatic/SCUS-94454/gcc/win-x64/cg5_84eaacd4`
- Static manifest entries: **8295**

- Base BIOS native dispatch entries: **1314**; relocated kernel body ranges: **37**
- Combined metrics below count both the play-free overlay cache and the separately generated, live-byte-guarded base BIOS.

## vs played vault (most complete needed-set)

- Quarantined proven all-zero played ranges: **1** (excluded from the needed set)
- Manifest entries in the full-playthrough vault: **1855**
- Discovered by static: **1270** (**68.5%** entry-level recall)
- Covered by compiled static code ranges: **1811** (**97.6%** code-range recall)
  - Code-range recall answers whether the played entry PC is in byte-guarded native code. Exact-entry recall is stricter manifest granularity; runtime fragment caches can contain one entry per instruction, so it substantially understates broad static shards.
- Byte-identical (entry+code_crc): **1149** (**61.9%**) _(cg-version differences lower this vs entry-level)_
- **MISSED exact entries: 585**
- **TRUE CODE-RANGE GAPS: 44** played entry PCs outside all compiled static ranges

### Combined with base recompiled BIOS

- Exact native dispatch entries: **1294** (**69.8%**)
- Covered by native code ranges: **1855** (**100.0%**)
- **COMBINED CODE-RANGE GAPS: 0**

#### Combined gaps grouped by region

- None.

### Code-range gaps grouped by overlay region

- region `0x80000000`: 18 misses
  ```
  800000A0 800000B0 800000C0 800005C4 800005E0 80000600 80000650 80000CF0
  80000D00 80000DE8 80000DF8 80000E08 80000E10 80000E18 80000E20 80000E28
  80000E38 80000E44
  ```
- region `0x80001000`: 8 misses
  ```
  80001444 800015D8 80001E44 80001E80 80001E98 80001EA8 80001F10 80001F3C
  ```
- region `0x80002000`: 9 misses
  ```
  800020D4 80002C94 80002CB0 80002CD4 80002D24 80002DA8 80002DD8 80002DF0
  80002EFC
  ```
- region `0x80003000`: 2 misses
  ```
  800030C8 80003E80
  ```
- region `0x80004000`: 4 misses
  ```
  800043D0 800043E8 8000445C 80004C70
  ```
- region `0x80006000`: 3 misses
  ```
  8000641C 80006594 80006B90
  ```

## vs persisted live-session gaps (monotonic roll-forward)

- Dispatch entries the persisted session exercised: **826**
- Discovered by current static: **236** (**28.6%** entry-level recall)
- Covered by current static code ranges: **731** (**88.5%** code-range recall)
- Including base BIOS native code ranges: **808** (**97.8%**)
- **MISSED live: 590**
- Provenance: caller explicitly asserted that the current static entry set retains every prior static entry.
