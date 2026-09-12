# Original-disc AOT releases

Both regions consume the shared psxrecomp [AOT pipeline](../psxrecomp-v4/docs/AOT_SHARDING.md).
Game-owned profiles `aot/usa.json` and `aot/ita.json` contain disc identities,
loader instructions, filename tables, source addresses and composition facts.
Extraction, identification methods, compilation and auditing live in psxrecomp.

Each region accounts for MAIN.EXE and all 28 BIN images, including all 22 area
overlays, START, GAME, DEMO, SOP, OPN and CRD. The US profile produces 54 recipes
(including the small START overlay previously missed by the generic scan); the
Italian profile produces 77. Recipes also include verified simultaneous images
and the shared BIOS resident manifest. Counts describe inventory, not proof of
complete executable coverage.

US GAME and area images are adjacent. Italian GAME ends before the area load
address; the shared composition method excludes that 372-byte gap from known
producer intervals. Each region verifies its own original loader words and
filename table. Addresses and hashes are never borrowed from another region.

## Build and package

Provide original discs at the paths in `game.toml` / `game_ita.toml` and use the
pinned framework emitter. Both packagers always run fresh extraction and compile
every recipe in isolation, then audit every native pair against original bytes.
They require native coverage for every recipe and stage every audited pair.
Missing inputs, changed evidence, compiler failures and incomplete audits stop
packaging. Existing gameplay captures and caches are not inputs.

```powershell
./tools/package_release.ps1 -Variant usa -BuildDir build-release-usa
./tools/package_release.ps1 -Variant ita -BuildDir build-release-ita
```

`-RecompilerBuildDir` selects a build directory inside `psxrecomp-v4`.
`-SkipRegen` only skips base-game regeneration; original-disc AOT is mandatory.
`-Version` overrides the default in `packaging/release/VERSION`.

```sh
bash tools/package_appimage.sh --variant usa --build-dir /path/to/build-usa
bash tools/package_appimage.sh --variant ita --build-dir /path/to/build-ita
```

Linux builds use the native Linux compiler and separate regional runtime build
directories. `--skip-build` can reuse a completed runtime build, but still
extracts and builds every AOT recipe. Windows DLLs cannot be used by Linux.
The framework derives the correct namespace from the packaged runtime config;
renaming cache directories does not make differing code-generation settings
compatible. Both packagers include `AOT_CACHE_AUDIT.json` and SHA-256 sidecars.
No pair count from an earlier release is treated as an acceptance threshold.

For extraction-only discovery:

```sh
python psxrecomp-v4/tools/aot_overlay_pipeline.py extract --profile aot/usa.json --game-toml game.toml --recompiler RECOMPILER --work-dir build-aot-inputs
python psxrecomp-v4/tools/aot_overlay_pipeline.py extract --profile aot/ita.json --game-toml game_ita.toml --recompiler RECOMPILER --work-dir build-aot-inputs-ita
```

Generated recipes and code contain game bytes and remain private. Keep the
flavor-0 baseline aligned across runtime and cache. Other flavors need their
own native artifacts and validation. Prior owner spot checks passed mining,
water temple and Kujarra Ranch; further gameplay remains useful. Guard audits
establish provenance and ABI validity, not native semantics or full coverage.

Validation on 2026-09-11 rebuilt every configured recipe from fresh original-disc
inputs: USA produced 79 audited native pairs / 14,619 manifest rows; Italian
produced 95 / 20,495. Both audits established native entries for every recipe.
The shared compiler now separates invalid speculative branch candidates from
tool failures, and accepts an empty primary scan only when guarded fragments
serve every requested root. Missing roots and other compilation errors remain
fatal. These counts are validation results, not future release thresholds.
