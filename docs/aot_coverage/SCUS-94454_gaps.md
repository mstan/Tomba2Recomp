# AOT static-coverage recall — SCUS-94454

_How much of the played reference set did the play-free static extractor reproduce, and how much lies in compiled static code?_

- Static shard cache: `build-aot/cache/SCUS-94454/tcc/win-x64/cg5_b0538be8`
- Static manifest entries: **6720**

- Base BIOS native dispatch entries: **1314**; relocated kernel body ranges: **37**
- Combined metrics below count both the play-free overlay cache and the separately generated, live-byte-guarded base BIOS.

## vs played vault (most complete needed-set)

- Quarantined proven all-zero played ranges: **1** (excluded from the needed set)
- Manifest entries in the full-playthrough vault: **1855**
- Discovered by static: **1158** (**62.4%** entry-level recall)
- Covered by compiled static code ranges: **1716** (**92.5%** code-range recall)
  - Code-range recall answers whether the played entry PC is in byte-guarded native code. Exact-entry recall is stricter manifest granularity; runtime fragment caches can contain one entry per instruction, so it substantially understates broad static shards.
- Byte-identical (entry+code_crc): **1055** (**56.9%**) _(cg-version differences lower this vs entry-level)_
- **MISSED exact entries: 697**
- **TRUE CODE-RANGE GAPS: 139** played entry PCs outside all compiled static ranges

### Combined with base recompiled BIOS

- Exact native dispatch entries: **1182** (**63.7%**)
- Covered by native code ranges: **1760** (**94.9%**)
- **COMBINED CODE-RANGE GAPS: 95**

#### Combined gaps grouped by region

- region `0x8008A000`: 3 misses
  ```
  8008A3EC 8008A4EC 8008A538
  ```
- region `0x80106000`: 3 misses
  ```
  8010696C 80106AC4 80106F80
  ```
- region `0x80107000`: 7 misses
  ```
  80107400 80107790 801079AC 80107AFC 80107D3C 80107E20 80107F3C
  ```
- region `0x80108000`: 13 misses
  ```
  8010810C 801084F8 80108624 801086E0 80108720 80108784 8010882C 801088D8
  801089C4 80108A60 80108B0C 80108BE4 80108CAC
  ```
- region `0x8018A000`: 11 misses
  ```
  8018A1E8 8018A238 8018A260 8018A274 8018A288 8018A29C 8018A300 8018A428
  8018AEB8 8018AF34 8018AFF0
  ```
- region `0x8018B000`: 15 misses
  ```
  8018B020 8018B1A4 8018B478 8018B660 8018B7E4 8018BA68 8018BD30 8018BDD0
  8018BDEC 8018BDFC 8018BE1C 8018BE40 8018BF08 8018BF74 8018BFE8
  ```
- region `0x8018C000`: 20 misses
  ```
  8018C018 8018C02C 8018C040 8018C054 8018C0F0 8018C0FC 8018C108 8018C114
  8018C128 8018C13C 8018C150 8018C164 8018C2B4 8018C2BC 8018C2E0 8018C4B4
  8018C790 8018C820 8018CCCC 8018CE40
  ```
- region `0x8018D000`: 7 misses
  ```
  8018D26C 8018D418 8018D74C 8018DB28 8018DC2C 8018DD38 8018DFEC
  ```
- region `0x8018E000`: 7 misses
  ```
  8018E2B4 8018E54C 8018E6BC 8018E95C 8018EC00 8018ED94 8018EFDC
  ```
- region `0x8018F000`: 9 misses
  ```
  8018F280 8018F414 8018F548 8018F660 8018F818 8018F854 8018FA88 8018FBCC
  8018FBF8
  ```

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
- region `0x8008A000`: 3 misses
  ```
  8008A3EC 8008A4EC 8008A538
  ```
- region `0x80106000`: 3 misses
  ```
  8010696C 80106AC4 80106F80
  ```
- region `0x80107000`: 7 misses
  ```
  80107400 80107790 801079AC 80107AFC 80107D3C 80107E20 80107F3C
  ```
- region `0x80108000`: 13 misses
  ```
  8010810C 801084F8 80108624 801086E0 80108720 80108784 8010882C 801088D8
  801089C4 80108A60 80108B0C 80108BE4 80108CAC
  ```
- region `0x8018A000`: 11 misses
  ```
  8018A1E8 8018A238 8018A260 8018A274 8018A288 8018A29C 8018A300 8018A428
  8018AEB8 8018AF34 8018AFF0
  ```
- region `0x8018B000`: 15 misses
  ```
  8018B020 8018B1A4 8018B478 8018B660 8018B7E4 8018BA68 8018BD30 8018BDD0
  8018BDEC 8018BDFC 8018BE1C 8018BE40 8018BF08 8018BF74 8018BFE8
  ```
- region `0x8018C000`: 20 misses
  ```
  8018C018 8018C02C 8018C040 8018C054 8018C0F0 8018C0FC 8018C108 8018C114
  8018C128 8018C13C 8018C150 8018C164 8018C2B4 8018C2BC 8018C2E0 8018C4B4
  8018C790 8018C820 8018CCCC 8018CE40
  ```
- region `0x8018D000`: 7 misses
  ```
  8018D26C 8018D418 8018D74C 8018DB28 8018DC2C 8018DD38 8018DFEC
  ```
- region `0x8018E000`: 7 misses
  ```
  8018E2B4 8018E54C 8018E6BC 8018E95C 8018EC00 8018ED94 8018EFDC
  ```
- region `0x8018F000`: 9 misses
  ```
  8018F280 8018F414 8018F548 8018F660 8018F818 8018F854 8018FA88 8018FBCC
  8018FBF8
  ```

## vs live capture history

- Sources: **1 capture file + verified append-only history**
- FNV-verified immutable snapshots: **23**; invalid records: **0**
- Superseded session snapshots **not FNV-verified** (their parsed entry sets were subsets of a verified head): **612** (868.8 MiB parsed under bounds)
- Dispatch entries exercised: **888**
- Discovered by static: **126** (**14.2%** entry-level recall)
- Covered by compiled static code ranges: **779** (**87.7%** code-range recall)
- Overlay-only true code-range gaps: **109**
- Including base BIOS native code ranges: **859** (**96.7%**)
- Combined true code-range gaps: **29**
- Exact-entry misses (diagnostic; may be interior fragments): **762**
