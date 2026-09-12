# Preparing original-disc AOT releases

See the framework's [AOT sharding guide](https://github.com/RetroPortingToolKit/psxrecomp/blob/master/docs/AOT_SHARDING.md)
for the reusable discovery/compiler/cache boundary. This repository supplies
Tomba 2 regional loader facts and a release audit. Full decompilation is not
required. Use the pinned framework/UI and keep original discs, extracted bytes,
generated code, captures and saves outside Git.

Place the original disc and boot executable at the local paths in `game.toml`
or `game_ita.toml`, then build `psxrecomp-game` from the pinned framework.

```powershell
$python = 'C:/msys64/mingw64/bin/python.exe'
$recompiler = 'psxrecomp-v4/recompiler/build-t2/psxrecomp-game.exe'
& $python psxrecomp-v4/tools/aot_overlay_spike/extract_generic.py `
  --game-toml game.toml --recompiler $recompiler `
  --out aot/usa-generic.json --tmp aot/usa-extract
& $python tools/prepare_usa_aot_inventory.py `
  --disc 'tomba2/Tomba! 2 - The Evil Swine Return (USA).cue' `
  --generic-records aot/usa-generic.json --out-dir aot/usa
```

For Italian, extract using `game_ita.toml` to `aot/ita-generic.json`, then:

```powershell
& $python tools/prepare_italian_aot.py `
  --disc 'tombi2/Tombi! 2 (Italy).cue' `
  --generic-records aot/ita-generic.json --out-dir aot/ita
```

The US verifier matches recipes to original MAIN/BIN extents and all 22 area
destinations. The Italian recipe verifies START's filename table and MAIN's
loader before adding START/GAME and shared/area combinations. GAME leaves a
372-byte gap before Italian areas; known producer intervals exclude it. The
separate BIOS-resident producer retains its profile/manifest provenance.
The baseline produced 53 US and 77 Italian recipes, all with `guard_bytes=0`.

Compile each inventory job's `input` with the corresponding regional config:

```powershell
& $python psxrecomp-v4/tools/compile_overlays.py `
  --captures aot/usa/runtime-inputs/000.json --game-toml game.toml `
  --recompiler $recompiler --runtime-include psxrecomp-v4/runtime/include `
  --out-dir build-aot/cache --compiler gcc `
  --gcc C:/msys64/mingw64/bin/gcc.exe --flavor 0 --jobs 1
```

One recipe per invocation bounds discovery; broader cross-area enrichment is a
separate choice. Preserve logs and review nonzero outcomes. Italian MAIN rejected
two speculative groups; START built its proven entry through a supplement.
Those partial outcomes were explicitly reviewed, not silently called complete.

Audit every published pair, including supplements:

```powershell
& $python tools/audit_aot_cache.py --recompiler $recompiler `
  --game-toml packaging/release/game.toml --cache-root build-aot/cache `
  --inventory aot/usa/runtime-input-inventory.json `
  --flavor 0 --output aot/usa-cache-audit.json
```

Use the Italian inventory/config for Italian. This checks namespace/flavor,
ABI, exports, pair identity and all guarded bytes against known inputs. It
does not prove native semantics or replace verifying the input recipes.

The Windows packager requires an inventory and repeats this audit after staging:

```powershell
./tools/package_release.ps1 -Variant usa -Version v0.0.10 `
  -BuildDir build-release-usa -CacheBuildDir build-aot `
  -AotInventory aot/usa/runtime-input-inventory.json -ExpectedAotPairs 334
./tools/package_release.ps1 -Variant ita -Version v0.0.10-ita.1 `
  -BuildDir build-release-ita -CacheBuildDir build-aot `
  -AotInventory aot/ita/runtime-input-inventory.json -ExpectedAotPairs 74
```

Use `-SkipRegen` only for already generated, validated code from the same emitter.
`-RecompilerBuildDir` can select another configured directory relative to
`psxrecomp-v4`. Counts above describe this checkpoint, not permanent minima.
`AOT_CACHE_AUDIT.json` ships pair hashes and metadata, without game bytes.

For Linux, build the pinned recompiler and runtime on Linux, and repeat the
same inventory compilation with native `python3` and `gcc`. Regenerate the
inventories with Linux paths. Windows DLLs cannot be used by a Linux runtime;
the shared compiler selects the Linux cache namespace and `.so` format.
Use separate US and Italian runtime build directories so their mod catalogs
remain independent. Then package each verified cache:

```sh
OVERLAY_CACHE_DIR=/path/to/linux/cache bash tools/package_appimage.sh \
  --variant usa --version v0.0.10 --build-dir /path/to/build-usa --skip-build \
  --aot-inventory /path/to/aot/usa/runtime-input-inventory.json
OVERLAY_CACHE_DIR=/path/to/linux/cache bash tools/package_appimage.sh \
  --variant ita --version v0.0.10-ita.1 --build-dir /path/to/build-ita --skip-build \
  --aot-inventory /path/to/aot/ita/runtime-input-inventory.json
bash tools/test_appimage_layout.sh --variant usa --version v0.0.10 \
  release-linux/Tomba2Recomp-v0.0.10-linux-x86_64.AppImage
bash tools/test_appimage_layout.sh --variant ita --version v0.0.10-ita.1 \
  release-linux/Tombi2Recomp-ita-v0.0.10-ita.1-linux-x86_64.AppImage
```

The AppImage packager repeats the original-input audit after staging and emits
a SHA-256 sidecar. Its audit receipt is also copied into the writable data
directory. Layout checks verify regional assets, native libraries, and retention
of user files across repeated launches; boot smoke checks remain a separate step.

Both builds use the validated flavor-0 baseline with geometry correction off.
Other flavors need their own native artifacts and validation. Keep packaged
code-generation settings aligned; renaming a cache directory does not make it
compatible. Test extracted packages, disc/BIOS selection, native loading and
area transitions with runtime compilation disabled. Owner spot checks passed;
complete static coverage and a full playthrough remain unproven.
