# Technical Documentation: Italian PAL Release (SCES-02686) Port & Offset Derivations

This document provides a comprehensive, instruction-by-instruction and byte-for-byte mathematical derivation and verification of all memory offsets, opcodes, registers, and runtime configuration parameters introduced for the Italian PAL release of **Tombi! 2 - The Evil Swine Return** (`SCES-02686`).

All derivations were extracted and verified against the original retail PlayStation binary images (`SCES_026.86`, `MAIN.EXE`, and the overlay archives in `BIN/A*.BIN` extracted from Track 1 of the Italian release disc, SHA-256 `b9c8ff05f265f2ec359bc559b0731109de15c0a0d0d036b0da23ddbf98a19188`).

---

## 1. Overview & Media Identity

| Property | US NTSC-U (`SCUS-94454`) | Italian PAL (`SCES-02686`) | Verification Evidence / Method |
| :--- | :--- | :--- | :--- |
| **Title Name** | `Tomba! 2 - The Evil Swine Return` | `Tombi! 2 - The Evil Swine Return` | European retail release naming |
| **Serial / Game ID** | `SCUS-94454` | `SCES-02686` | `SYSTEM.CNF` & disc metadata |
| **Disc Track 1 SHA-256** | `8e9568388155...` | `b9c8ff05f265f2ec359bc559b0731109de15c0a0d0d036b0da23ddbf98a19188` | Exact Redump Track 1 hash |
| **Boot Executable** | `tomba2/SCUS_944.54` | `tombi2/SCES_026.86` | Extracted bootloader EXE |
| **Entry Point (`entry_pc`)** | `0x80018B6C` | `0x80017B90` | PS-X EXE header field `PC0` |
| **Text Segment Base (`load_address`)**| `0x80010000` | `0x80010000` | PS-X EXE header field `TextAddr` (KSEG0) |
| **Text Segment Size (`text_size`)** | `0x00028800` (165,888 B) | `0x00028000` (163,840 B) | PS-X EXE header field `TextSize` |
| **Stack Pointer Base (`stack_base`)** | `0x801FFFF0` | `0x801FFFF0` | PS-X EXE header field `StackAddr` |
| **Audit Region ROM Range** | `0x800` .. `0x29000` | `0x800` .. `0x28800` | Header skip (`0x800`) + `text_size` (`0x28000`) |

---

## 2. Recompiler Configuration (`game_ita.toml`)

The Italian release boot executable (`SCES_026.86`) is a minimal loader that initializes kernel state and streams runtime overlays from disc.

* **Recompiler Seed (`seeds/ghidra_funcs_ita.txt`)**:
  * Set to entry point `0x80017B90`.
  * Dynamic control-flow walking discovers all 3,022 boot functions and emits 6 split translation shards (`SCES_026.86_full_00.c` to `05.c`) and the dispatch table (`SCES_026.86_dispatch.c`).
* **Runtime Identity**:
  * `debug_port = 4516` (US uses `4515`): Distinct TCP port allowing side-by-side execution and debugging of both releases without socket collision.
  * `memcard_dir = "saves_ita"`: Dedicated save-card directory isolating PAL saves.
  * `controller = "digital"` / `lock_mode = true`: Mirrors US behavior to bypass DualShock analog configuration handshake bugs.

---

## 3. Load Acceleration (`[load_accel.vsync_query]`)

The PsyQ runtime `VSync(mode)` function query loop is accelerated by intercepting side-effect-free MMIO reads while preserving exact guest CPU instruction timing and IRQ cadence.

### Disassembly Analysis (`SCES_026.86` at `0x80016E70`):
```mips
0x80016E70: lui   $v0, 0x8002          # Function entry point
0x80016E74: lw    $v0, 0x4DA0($v0)     # GPUSTAT pointer read from 0x80024DA0
0x80016E78: lui   $a1, 0x8002
0x80016E7C: lw    $a1, 0x4DA4($a1)     # Timer1 pointer read from 0x80024DA4
0x80016E80: addiu $sp, $sp, -40
0x80016E84: sw    $ra, 32($sp)
0x80016E88: sw    $s1, 28($sp)
0x80016E8C: sw    $s0, 24($sp)
0x80016E90: lw    $s0, 0($a0)
0x80016E94: lw    $v0, 0($v0)          # GPU status MMIO poll
...
# VBlank counter return on query (mode == -1):
0x80016F48: lui   $v0, 0x8002
0x80016F4C: lw    $v0, 0x5ED8($v0)     # Guest VBlank counter at 0x80025ED8
0x80016F50: jr    $ra
0x80016F54: addiu $sp, $sp, 40
```

### TOML Definition:
```toml
[load_accel.vsync_query]
func = "0x80016E70"
counter_addr = "0x80025ED8"
gpustat_ptr_addr = "0x80024DA0"
timer1_ptr_addr = "0x80024DA4"
timer1_cache_addr = "0x80024DA8"
```
*(Corresponding US addresses: `func = 0x80017E4C`, `counter = 0x800267B4`, `gpustat = 0x8002567C`, `timer1_ptr = 0x80025680`, `timer1_cache = 0x80025684`).*

---

## 4. Overlay Interpreter Native Blocklist (`overlay_native_block`)

Timing-sensitive splash and FMV task initialization routines must remain on the interpreter to avoid race conditions when transitioning from the Whoopee Camp logo to the opening movie.

### Mappings in `MAIN.EXE`:
* US `0x80096A90` -> **Italian PAL `0x80097818`** (`addiu $sp, $sp, -24`, FMV sector queue task setup)
* US `0x80052078` -> **Italian PAL `0x800529D8`** (`addiu $sp, $sp, -24`, Splash screen state timer)
* US `0x800520C0` -> **Italian PAL `0x80052A20`** (`jal 0x80081628`, Splash task teardown & movie handoff)

```toml
overlay_native_block = [
  "0x80097818",
  "0x800529D8",
  "0x80052A20",
]
```

---

## 5. Widescreen Frustum & Culling Derivations

### 5.1 Ground-Plane Resident Proximity Trigger (`bias_sites` & `range_sites`)

In `MAIN.EXE`, the function **`FUN_8006A438`** (corresponding to US `FUN_80069B6C`) computes the player-relative bounding window for actor simulation on the ground plane.

#### Disassembly (`MAIN.EXE` `0x8006A4C4` .. `0x8006A52C`):
```mips
# --- Y-Axis (Vertical Proximity) ---
0x8006A4C4: lhu   $v1, 0x36($a0)       # Player Y
0x8006A4C8: lhu   $v0, 0x34($a1)       # Object Y
0x8006A4D0: subu  $v0, $v0, $v1        # delta Y
0x8006A4D4: addiu $v0, $v0, 230        # Bias Y = 230  --> UNTOUCHED (Vanilla safety)
0x8006A4DC: sltiu $v0, $v0, 461        # Range Y = 461 --> UNTOUCHED (Vanilla safety)
0x8006A4E0: beqz  $v0, 0x8006A534      # Reject if outside vertical threshold

# --- X-Axis (Horizontal Ground Plane) ---
0x8006A4E8: lhu   $v0, 0x2C($a1)       # Object X
0x8006A4EC: lhu   $v1, 0x2E($a0)       # Player X
0x8006A4F4: subu  $v0, $v0, $v1        # delta X
0x8006A4F8: addiu $v0, $v0, 230        # Bias X = 230  --> BIAS SITE X (0x8006A4F8)
0x8006A500: sltiu $v0, $v0, 461        # Range X = 461 --> RANGE SITE X (0x8006A500)
0x8006A504: beqz  $v0, 0x8006A534      # Reject if outside horizontal threshold

# --- Z-Axis (Depth Ground Plane) ---
0x8006A50C: lhu   $v0, 0x30($a1)       # Object Z
0x8006A510: lhu   $v1, 0x32($a0)       # Player Z
0x8006A518: subu  $v0, $v0, $v1        # delta Z
0x8006A51C: addiu $v0, $v0, 230        # Bias Z = 230  --> BIAS SITE Z (0x8006A51C)
0x8006A524: jr    $ra
0x8006A528: sltiu $v0, $v0, 461        # Range Z = 461 --> RANGE SITE Z (0x8006A528, delay slot)
```

* **Rationale**: The Y-axis pair (`0x8006A4D4` / `0x8006A4DC`) is preserved vanilla so vertical gameplay triggers are never artificially widened. The X/Z ground-plane pairs receive `activation_guard_pixels = 256` to smoothly spawn actors as the camera pans horizontally.

---

### 5.2 Screen-X Render Funnel Reject (`screen_x_sites`)

In non-GTE polygon rasterizers, screen bounds are checked explicitly:

#### Disassembly (`MAIN.EXE` `0x8003EB24`):
```mips
0x8003EB18: addiu $s0, $s0, 12
0x8003EB1C: addiu $s1, $s1, 64
0x8003EB20: andi  $v0, $s1, 0xFFFF
0x8003EB24: sltiu $v0, $v0, 320        # Screen-X comparison (Opcode: 0x2C420140)
0x8003EB28: bnez  $v0, 0x8003EA58
```
* **Site**: `screen_x_sites = ["0x8003EB24"]` (replaces US `0x8003E228`).
* **PAL Screen Immediates**:
  * `screen_w_imms = ["0x140", "0x141", "0x142"]` (320, 321, 322).
  * `screen_h_imms = ["0xF0", "0xFE", "0xFF", "0x100", "0x101", "0x102", "0x103"]` (supports 256-line PAL display height).
  * `guard_pixels = 52` (covers horizontal reveal up to 426 px at 16:9 on PAL).

---

### 5.3 Terrain Angle Producers (`[[widescreen.cull.angle]]`)

Terrain-cell selection quads load angular half-extents (12-bit values) and store them into terrain descriptors (`sw $reg, 0x4D98 / 0x4D9C`). The recompiler widens tan(angle) by the horizontal aspect scale factor.

| # | Address (IT) | Binary Source & Offset | Opcode | Disassembly | Angle (Dec) | US Equivalent |
| :- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | `0x8010B5E8` | `SOP.BIN` (0x011A4) / `A03.BIN` | `0x240301C7` | `addiu $v1, $zero, 455` | 455 | `0x8010E9A0` |
| 2 | `0x801405D0` | `A00.BIN` (0x3618C) | `0x24020155` | `addiu $v0, $zero, 341` | 341 | `0x8013F138` |
| 3 | `0x80140628` | `A00.BIN` (0x361E4) | `0x240301C7` | `addiu $v1, $zero, 455` | 455 | `0x8013F190` |
| 4 | `0x801406BC` | `A00.BIN` (0x36278) | `0x2402013E` | `addiu $v0, $zero, 318` | 318 | `0x8013F224` |
| 5 | `0x801406DC` | `A00.BIN` (0x36298) | `0x240201C7` | `addiu $v0, $zero, 455` | 455 | `0x8013F244` |
| 6 | `0x80130360` | `A01.BIN` (0x25F1C) | `0x2403012C` | `addiu $v1, $zero, 300` | 300 | `0x8012EEB8` |

---

### 5.4 Camera Aspect Cones & Model Queues (`[widescreen.cull.aspect_cone]`)

In `MAIN.EXE`, **`FUN_80077A7C`** (corresponding to US `FUN_8007712C`) evaluates model visibility cones against the camera forward vector.

#### Scratchpad State & Register Mapping:
* `forward_addr = "0x1F8000E8"`: Signed Q12 camera forward vector (X, Z, Y) loaded from scratchpad offset `0x1F8000D0 + 0x18`.
* `object_reg = 19` (`$s3`): Pointer to the candidate model (`0x80077A84: addu $s3, $a0, $zero`).
* `x_reg = 16` (`$s0`), `z_reg = 17` (`$s1`), `y_reg = 18` (`$s2`): Camera coordinates (`0x80077AAC..0x80077ABC`).
* `object_type_offset = 12`: Model type identifier byte (`0x80077B44: lbu $v1, 12($s3)`).

#### Scratchpad Model Insertion Queues:
* **Queue 1** (`0x1F800144`): Capacity **24** (`0x80077F84: slti $v0, $v0, 24`), Type mask `0x00000204` (Types 2 and 9).
* **Queue 2** (`0x1F800150`): Capacity **40** (`0x80077FBC: slti $v0, $v0, 40`), Type mask `0x00000010` (Type 4).
* **Queue 3** (`0x1F80015C`): Capacity **28** (`0x80077FF4: slti $v0, $v0, 28`), Type mask `0x00000020` (Type 5).

#### The Six Main Actor Reject Sites:
Signed `SLTI` comparisons testing dot(fwd, delta) < threshold:
1. `0x80077C18`: `0x28620370` (`slti $v0, $v1, 880`) -> Q10 Cosine: **880** (US `0x800772D4`)
2. `0x80077CAC`: `0x28620358` (`slti $v0, $v1, 856`) -> Q10 Cosine: **856** (US `0x80077368`)
3. `0x80077D58`: `0x28620358` (`slti $v0, $v1, 856`) -> Q10 Cosine: **856** (US `0x80077414`)
4. `0x80077DEC`: `0x28620370` (`slti $v0, $v1, 880`) -> Q10 Cosine: **880** (US `0x800774A8`)
5. `0x80077E80`: `0x28620350` (`slti $v0, $v1, 848`) -> Q10 Cosine: **848** (US `0x8007753C`)
6. `0x80077F14`: `0x28620368` (`slti $v0, $v1, 872`) -> Q10 Cosine: **872** (US `0x800775D0`)

---

### 5.5 Per-Model / Child Part Cone & `cosine_threshold` Algebraic Derivation

In `MAIN.EXE`, **`FUN_8002B9B4`** (corresponding to US `FUN_8002B278`) evaluates individual model parts and child meshes.

#### MIPS Arithmetic Sequence (`MAIN.EXE` `0x8002BA7C` .. `0x8002BAA4`):
```mips
0x8002BA7C: sll   $v1, $a1, 2          # v1 = 4 * d
0x8002BA88: sll   $v0, $a1, 5          # v0 = 32 * d
0x8002BA8C: subu  $v0, $v0, $v1        # v0 = 32d - 4d = 28d
0x8002BA90: sll   $v0, $v0, 2          # v0 = 28d * 4  = 112d
0x8002BA94: subu  $v0, $v0, $v1        # v0 = 112d - 4d = 108d
0x8002BA98: sll   $v0, $v0, 2          # v0 = 108d * 4  = 432d
0x8002BA9C: subu  $v0, $v0, $v1        # v0 = 432d - 4d = 428d
0x8002BAA0: sll   $v0, $v0, 3          # v0 = 428d * 8  = 3424 * d  (0xD60 in hex)
0x8002BAA4: slt   $a0, $a0, $v0        # Comparison: dot_product < 3424 * distance
```

#### Mathematical Proof:
1. The game derives the integer scalar:
   K = 8 * (4 * (4 * (32 - 4) - 4) - 4) = 8 * (4 * 108 - 4) = 8 * 428 = 3424 (0xD60 in hex)
2. In Q12 fixed-point math (4096 = 1.0), 3424 / 4096 = 0.8359.
3. The `psxrecomp` aspect cone system operates in **Q10 units** (1024 = 1.0):
   cosine_threshold = 3424 / 4 = 856 (0x358 in hex)

#### TOML Definition:
```toml
[[widescreen.cull.aspect_cone.sites]]
address = "0x8002BAA4"
expected = "0x0082202A"
cosine_threshold = 856
object_reg = 20 # s4
x_reg = 19      # s3
z_reg = 18      # s2
y_reg = 17      # s1
queue_guard = false
```

---

## 6. Built-in Mod Manifests & Test Coverage

The Italian release uses localized, Italian-only package manifests for the
preloaded user-facing enhancements:

* `tombi2.enhancement.widescreen`
* `tombi2.experimental.interpolated-frame-rate`
* `tombi2.enhancement.skip-fmvs`

Each Italian package declares the SCES-02686 target:
```toml
[[target]]
game_id = "SCES-02686"
disc_sha256 = "b9c8ff05f265f2ec359bc559b0731109de15c0a0d0d036b0da23ddbf98a19188"
```
The test suite in `tests/test_preloaded_mods.cpp` validates declarative
resolution for the 4 US packages and the 3 localized Italian packages. The
Italian target intentionally has no Debug Menu package until its PAL hook and
signature addresses are mapped and validated.

---

## 7. Developer Debug Menu PAL Status

The developer debug menu payload (`tomba2_debug_menu_payload.h`) was originally authored for `SCUS-94454`.

The Italian PAL release does not expose this package for now. Its manifest
target remains US-only because the payload detours and callee signatures have
not been ported to the SCES-02686 overlay layout.

---

## 8. Video Enhancements & PGXP Precision Configuration

In `game_ita.toml`, the `[video]` section explicitly sets:
```toml
[video]
geometry_correction = true
pgxp_tolerance      = -1.0
```

### Rationale & Runtime Lifecycle Analysis:

1. **Overcoming the Initialization Override Bug (`geometry_correction = true`)**:
   * In `psxrecomp-v4/runtime/src/main.cpp`, `mod_runtime_activate_plugins()` executes during early startup (line 12657), invoking `builtin_pgxp_activate()` in `mod_builtin_pgxp.c` which calls `gte_geometry_correction_set(1)`.
   * However, subsequently in `main.cpp` (line 12888, and line 14297 upon returning from the launcher UI), the runtime applies:
     ```cpp
     gte_geometry_correction_set(g_video_geometry_correction);
     gpu_texture_correction_set(g_video_perspective_texturing);
     ```
   * If `geometry_correction` is omitted or set to `false` in `game_ita.toml`, `g_video_geometry_correction` evaluates to `0`. This **silently overrides and disables** the vertex precision engine that the PGXP mod just activated.
   * Explicitly configuring `geometry_correction = true` in `game_ita.toml` keeps the GTE vertex channel open, allowing the PGXP mod and sub-pixel precision to function as intended.

2. **Full Sub-Pixel Precision vs Conservative Clamping (`pgxp_tolerance = -1.0`)**:
   * By default, `psxrecomp-v4/runtime/src/pgxp.cpp` applies a conservative `0.5px` tolerance filter (`s_tolerance = 0.5f`), rejecting any sub-pixel vertex displacement greater than half a pixel back to the stock integer grid (`s_stats.tolerance_reject++`).
   * In Tomba 2, complex segmented meshes and winding 2.5D terrain paths trigger this tolerance clamp extensively, causing widespread fallback to uncorrected integer coordinates and rendering PGXP visually indistinguishable from stock.
   * Setting `pgxp_tolerance = -1.0` disables the tolerance gate (`s_tolerance < 0.0f`), allowing full sub-pixel floating-point vertex stability across all 3D geometry and completely eliminating polygon jitter.
