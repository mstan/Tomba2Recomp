# Tomba! 2 - The Evil Swine Return release notes

## v0.3.3 — shared optimization release

Fresh settings enable Internal Resolution at 4x. The Mods selector offers 1x through 32x; saved choices are preserved, and GPU texture and memory limits still apply. Disabling the mod selects native resolution.

This build includes the combined qualified engine optimizations and uses the shared release pipeline with selective runtime PGO. CD Speed and Fast Loading packages are excluded. Existing memory cards remain supported.

Automated gameplay validation for this release batch is limited to Ape Escape and Tomba 2. The other titles use build and package checks followed by owner visual approval; no per-title performance gain is claimed.

# Tomba! 2 v0.3.2 - High-resolution fishing line alignment

- Fixes the remaining string offset above the Fisherman's Village bucket at
  higher internal resolutions. Expanded line segments now start a world draw
  batch instead of inheriting the previous background batch's stretch.
- Reproduced from the owner's slot 01 and checked at 4:3, 16:9, 21:9 and
  adaptive window sizes, including camera movement and current-build save/load.
- Retains the immediate 4:3 pause and journal presentation from v0.3.1.

# Tomba! 2 v0.3.1 - Widescreen menus and fishing line

- Pause and journal menus use their authored 4:3 aspect immediately, including
  when restoring a current-build snapshot with the pause menu open.
- Background expansion no longer classifies world line segments as backdrops.
  The remaining high-resolution batch-state offset is fixed in v0.3.2.
- Checked on a fresh US new game at 4:3, 16:9, 21:9 and adaptive window sizes.

# Tomba! 2 v0.3.0 - Experimental Mario's Hat

The US Windows preview adds optional **Mario's Hat, EXPERIMENTAL 0.0.1**.
Enable it in Mods, select your original Super Smash Bros. 64 US 1.0 ROM,
and equip the costume in game. A verified ROM remains required even when
imported assets are already cached. No Nintendo ROM or imported asset is shipped.

- Imported Mario appearance, motions and voice cues, double jump, extended punch
  reach and native grab/throw interactions.
- Corrected facing, chest perch, post/ledge grab, hanging and ladder presentation.
- Diagnostics disabled in production, complete launcher fonts/artwork and
  current framework presentation controls retained.
- Preserves the accepted wall-clock presentation default instead of adding
  synchronized-swap stalls on a non-matching high-refresh monitor.
- Current-build save/load and native memory-card progression were tested on
  representative village routes. Full progression, all-stage performance and
  complete native coverage remain unproven; the mod defaults off.
- CD Speed and host-paced Fast Loading packages are excluded from releases.

This release targets the US disc on Windows x64. Existing Italian and Linux
release artifacts remain available separately.

# Tomba! 2 v0.2.1 - Startup loading fix

Fixes a startup hang immediately after the Whoopee Camp logo movie that could
occur with an already prepared native cache. Sound-bank and area workers retain
their original cooperative scheduling; the resident texture worker keeps fast
loading. The fix is enabled by default.

This patch preserves v0.2.0's adaptive scenery, texture correction, authentic
game timing and launcher. Windows x64 and Linux x86_64 packages include OpenBIOS;
players supply their own US disc. The Italian release is unchanged.

# Tomba! 2 v0.2.0 - Adaptive scenery and authentic performance

Adaptive widescreen and 1080p internal rendering now default on for the US disc
(SCUS-94454), alongside texture correction and Seamless Loading. Resize the
window to reveal more of the scene; gameplay retains its authored timing.

- Repairs missing and partly rendered actors, signs, trees, effects and terrain
  in the supported viewport paths, including the opening village.
- Keeps all 343 nonempty opening-area terrain cells resident and submitted,
  rather than selecting a small camera-dependent subset.
- Repairs the sea/cloud discontinuity at the original 4:3 composition boundary
  and thin terrain faces lost through integer projection rounding.
- Restores approximately 30 fresh scene frames per second in the measured
  village movement and jump sections with complete scenery. The original
  two-VBlank scene loop remains unchanged; these measurements are not a
  whole-game minimum-FPS guarantee.
- Uses the enhanced resident-loader and added-terrain execution implementations
  selected at build time. Working reference implementations remain buildable.
- Retains default-on fast loading. The first launch prepares resources from
  your own disc; prepared game-resource data is not shipped.
- Disables and hides interpolation and generic CD/host timing controls.

The owner approved the visibility, loading and authentic-rate candidate.
Windows x64 and Linux x86_64 packages include OpenBIOS and audited native
overlay candidates prepared from the original US inputs. Players supply their
own US disc. Complete native coverage and a full playthrough remain unproven;
interpreter and runtime-compilation fallback remain available for gaps.
The Italian release is unchanged.

# Tomba! 2 v0.1.0 - Seamless Loading

Seamless Loading is enabled by default for the US disc (SCUS-94454).
The first launch prepares verified resources from the player's disc and caches
them locally. Area transitions use resident resources at normal gameplay and
audio speed. Disable the mod to restore the original loader.

- Replaces the generic CD Speed and turbo-style Fast Loading options.
- Keeps cutscenes, music and speech on their normal playback paths.
- Excludes the experimental debug menu, whose patched code caused slowdown.
- Includes freshly audited native overlays for all original area images and
  shared code, including the opening prologue.

The owner accepted late-game area testing. This does not claim exhaustive
coverage of every event or compatibility with arbitrary asset replacements.
Windows x64 and Linux x86_64 packages include OpenBIOS; players provide their
own US disc. No disc resources, retail BIOS dump or player save is bundled.
The Italian version remains on its existing release and loading behavior.

## Previous release: v0.0.10 AOT overlays

Native overlay code is now prepared from the original discs before gameplay.
Both regions include candidates from all 22 area files, plus shared code.
The owner reported great performance in Italian water temple and Kujara Ranch.

| Windows x64 build | Disc | Audited native files | Manifest candidate rows |
| --- | --- | ---: | ---: |
| Tomba! 2 US | SCUS-94454 | 334 | 22,533 |
| Tombi! 2 Italian PAL | SCES-02686 | 74 | 14,920 |

These are native candidate counts, not exhaustive function counts or coverage
percentages. Each region uses its own bytes, load addresses and cache namespace.
Multiple recipes can reuse matching guarded functions without another DLL.

The framework recognizes an additional bounded switch-table scheduling pattern
and explicitly marks disc records as having no capture trailer. The packager
audits every staged pair against known original inputs. US packaged codegen
settings now match the validated build. Both regions use the tested baseline
with geometry correction disabled.

Validation includes original-input/guard and ABI checks, boot/attract testing,
mining movement/interactions and area round trips, and owner water temple and
Kujara Ranch checks. Historical captures were not AOT inputs or correctness
oracles. The static-cache tests had runtime compilation disabled.

Interpreter and runtime-compilation fallback remain available for gaps. Complete
static coverage and a full playthrough are unproven. One early Italian attract
watchdog abort was not reproduced in longer retesting; its cause is unresolved
and the watchdog remains enabled. Widescreen and other optional enhancements
are outside the baseline AOT gameplay checks.

Windows x64 ZIPs and Linux x86_64 AppImages require the matching original disc image. OpenBIOS
and its MIT notice are bundled; an optional supported retail BIOS may be
selected. Disc images, retail BIOS dumps, player saves and capture JSON are not
included. Keep a backup when moving memory cards to a new installation.
