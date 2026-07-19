# AOT static-coverage recall — SCUS-94454

_How much of the played reference set did the play-free static extractor reproduce, and how much lies in compiled static code?_

- Static shard cache: `build-aot/cache-fullstatic-b053/SCUS-94454/gcc/win-x64/cg5_b0538be8`
- Static manifest entries: **7428**

- Base BIOS native dispatch entries: **1314**; relocated kernel body ranges: **37**
- Combined metrics below count both the play-free overlay cache and the separately generated, live-byte-guarded base BIOS.

## vs played vault (most complete needed-set)

- Quarantined proven all-zero played ranges: **1** (excluded from the needed set)
- Manifest entries in the full-playthrough vault: **1855**
- Discovered by static: **1255** (**67.7%** entry-level recall)
- Covered by compiled static code ranges: **1811** (**97.6%** code-range recall)
  - Code-range recall answers whether the played entry PC is in byte-guarded native code. Exact-entry recall is stricter manifest granularity; runtime fragment caches can contain one entry per instruction, so it substantially understates broad static shards.
- Byte-identical (entry+code_crc): **1141** (**61.5%**) _(cg-version differences lower this vs entry-level)_
- **MISSED exact entries: 600**
- **TRUE CODE-RANGE GAPS: 44** played entry PCs outside all compiled static ranges

### Combined with base recompiled BIOS

- Exact native dispatch entries: **1279** (**68.9%**)
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

## vs live capture history

- Sources: **2 capture files + verified append-only history**
- FNV-verified immutable snapshots: **1**; invalid records: **0**
- Superseded session snapshots **not FNV-verified** (their parsed entry sets were subsets of a verified head): **97** (131.3 MiB parsed under bounds)
- Dispatch entries exercised: **712**
- Discovered by static: **123** (**17.3%** entry-level recall)
- Covered by compiled static code ranges: **623** (**87.5%** code-range recall)
- Overlay-only true code-range gaps: **89**
- Including base BIOS native code ranges: **700** (**98.3%**)
- Combined true code-range gaps: **12**
- Exact-entry misses (diagnostic; may be interior fragments): **589**

## vs pre-history persisted live-session gaps (separate monotonic roll-forward)

- This older needed set predates append-only history. It is reported separately because the old report retained misses, not enough addresses to compute an honest set union.

- Dispatch entries the persisted session exercised: **888**
- Discovered by current static: **152** (**17.1%** entry-level recall)
- Covered by current static code ranges: **796** (**89.6%** code-range recall)
- Overlay-only true code-range gaps: **92**
- Including base BIOS native code ranges: **876** (**98.6%**)
- Combined true code-range gaps: **12**
- Exact-entry misses (diagnostic; may be interior fragments): **736**
- Provenance: caller explicitly asserted that the current static entry set retains every prior static entry.
