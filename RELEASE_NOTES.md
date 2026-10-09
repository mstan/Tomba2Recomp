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
