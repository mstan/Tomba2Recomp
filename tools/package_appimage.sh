#!/usr/bin/env bash
# package_appimage.sh — build the Linux x86_64 AppImage release for a recomp title.
#
# Counterpart to tools/package_release.ps1 (Windows). Both packagers read the
# SAME sources so the two platforms cannot drift:
#
#   packaging/release/VERSION      the version string (single source of truth)
#   packaging/release/game.toml    the player-facing config
#   packaging/release/input.ini    the default controller mapping
#   packaging/release/START_HERE.txt
#
# Reproducibility:
#   * the version is never hardcoded here or in AppRun (AppRun's marker is
#     stamped from VERSION at package time),
#   * linuxdeploy/appimagetool are pinned by URL + SHA256 and verified,
#   * SOURCE_DATE_EPOCH is derived from the git commit (override to pin it),
#     and every staged file's mtime is normalised to it before squashing,
#   * the run prints SHA256 for the artifact.
#
# WSL: building an AppDir directly on a /mnt/<drive> DrvFs mount fails or
# silently degrades — symlinks need metadata mount options and the exec bit is
# not preserved. When the repo lives on /mnt we therefore stage the AppDir on
# the native Linux filesystem and copy only the finished .AppImage back to the
# Windows-visible output path. Pass --out to place it elsewhere; a Windows-style
# path (F:\... or F:/...) is translated with wslpath.
#
# Usage:
#   bash tools/package_appimage.sh                     # version from VERSION
#   bash tools/package_appimage.sh --version v0.0.7
#   bash tools/package_appimage.sh --out /mnt/f/drop   # or --out 'F:\drop'
#   bash tools/package_appimage.sh --skip-build        # reuse existing build dir
#
# There is deliberately no --allow-no-cache. Shipping an AppImage with no
# overlay cache means every player's first visit to every area runs on the
# dirty-RAM interpreter, and that relaxation is exactly what let a packager
# which staged ZERO shards look like a successful build. If a package genuinely
# has to ship without one, the framework tool takes
# --ship-without-overlay-cache-because '<reason>', which prints the reason.
#
# Prereqs: cmake, ninja or make, a C/C++ toolchain, libsdl2-dev,
# libgl1-mesa-dev, curl, ImageMagick (for the icon), python3, and a generated/
# tree (produced by the recompiler; generated/ is NOT tracked in this repo, so
# either run the recompiler first or copy a generated/ tree in).
#
# SHARED STAGING (bead beads-eio.3.102)
# -------------------------------------
# The overlay cache tag, the shard selection, the overlay toolchain and the mod
# catalog are NOT implemented here. They are implemented once, in the framework,
# and this script calls them:
#
#   psxrecomp-v4/tools/release_overlay_stage.sh   the surface sourced below
#   psxrecomp-v4/tools/release_stage.py           the single implementation
#
# They used to be implemented here, in a fork of a fork: three titles each
# carried their own copy of this file and each hand-built the cache tag inside a
# python heredoc that had just imported the module owning it. When the framework
# appended an `_f<flavor>` field to the tag, ApeEscapeRecomp's copy was
# hand-patched and this one was not, so `find -path "*/$cg_tag/*"` could not
# match the real `..._f0/` directory: shards evaluated to 0 and this script
# exited 1. That is why Tomba 2 v0.0.9 shipped Windows-only -- there is no
# AppImage asset in that release at all.
#
# So: do not reintroduce a tag format string, a shard filter, an extension list,
# an arch-abi string or a mod count here. runtime/tests/
# test_packagers_never_format_cache_tag.py in the framework fails if you do, and
# tools/test_no_local_cache_tag.sh runs it against this repository.
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Keep the original argv: the parse loop below shifts it away, and the
# low-priority re-exec has to pass the caller's arguments through intact.
orig_args=("$@")

version=""
out_dir=""
variant=usa
skip_build=0
build_dir=${BUILD_DIR:-"$root/build-appimage"}
# Leave two cores for the rest of the machine; packaging must not make the box
# unusable. Override with --jobs / BUILD_JOBS.
_cores=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
jobs=${BUILD_JOBS:-$(( _cores > 4 ? _cores - 2 : 2 ))}

while [ $# -gt 0 ]; do
    case "$1" in
        --version) version=$2; shift 2;;
        --variant) variant=$2; shift 2;;
        --out)     out_dir=$2; shift 2;;
        --build-dir) build_dir=$2; shift 2;;
        --jobs)    jobs=$2; shift 2;;
        --skip-build) skip_build=1; shift;;
        --nice) nice_level=$2; shift 2;;
        -h|--help) sed -n '2,36p' "$0"; exit 0;;
        *) echo "unknown arg: $1" >&2; exit 2;;
    esac
done

# Packaging a release should not make the machine unusable. Re-exec the whole
# script under `nice` once (children inherit it) unless already niced or told
# otherwise with --nice 0.
nice_level=${nice_level:-10}
if [ "$nice_level" -gt 0 ] && [ "${RECOMP_APPIMAGE_RENICED:-0}" != "1" ] \
   && command -v nice >/dev/null 2>&1; then
    export RECOMP_APPIMAGE_RENICED=1
    exec nice -n "$nice_level" "$0" ${orig_args[@]+"${orig_args[@]}"}
fi

[ -n "$version" ] || version=$(tr -d ' \t\r\n' < "$root/packaging/release/VERSION")
[ -n "$version" ] || { echo "empty version" >&2; exit 1; }

# Per-game identity (see packaging/release/app.conf). Everything below this
# point is title-neutral, so porting this script to another title is a copy
# plus that one file.
app_conf=$root/packaging/release/app.conf
[ -f "$app_conf" ] || { echo "missing $app_conf" >&2; exit 1; }
# shellcheck source=/dev/null
. "$app_conf"
# EXPECTED_MODS is deliberately NOT required: the catalog is verified against
# the manifest the BUILD publishes (psx_mod_catalog_<target>.txt), so no number
# is written down anywhere to go stale. See psx_add_mod_catalog below.
for v in APP_NAME EXE_NAME PAYLOAD_DIR DESKTOP_ID ENV_PREFIX ICON_SOURCE FRAMEWORK_DIR; do
    eval "val=\${$v:-}"
    [ -n "$val" ] || { echo "$app_conf does not set $v" >&2; exit 1; }
done
GAME_TOML=${GAME_TOML:-game.toml}
runtime_target=psx-runtime
generated_dir=generated
case "$variant" in
    usa) ;;
    ita)
        APP_NAME="Tombi! 2 (Italian) Recompiled"
        EXE_NAME="Tombi2Recomp-ita"
        PAYLOAD_DIR="tombi2recomp-ita"
        DESKTOP_ID="io.github.mstan.Tombi2RecompIta"
        ENV_PREFIX="TOMBI2_RECOMP_ITA"
        GAME_TOML="game_ita.toml"
        runtime_target=psx-runtime-ita
        generated_dir=generated_ita
        ;;
    *) echo "unknown variant: $variant" >&2; exit 2;;
esac

# --- path handling ---------------------------------------------------------
# Accept Windows-style output paths so the same command works from a WSL shell
# driven by a Windows tool.
to_unix_path() {
    case "$1" in
        [A-Za-z]:[/\\]*)
            if command -v wslpath >/dev/null 2>&1; then wslpath -u "$1"; else
                echo "cannot translate Windows path '$1' (wslpath missing)" >&2; exit 2
            fi;;
        *) printf '%s\n' "$1";;
    esac
}
[ -n "$out_dir" ] && out_dir=$(to_unix_path "$out_dir")
out_dir=${out_dir:-"$root/release-linux"}
mkdir -p -- "$out_dir"
out_dir=$(CDPATH= cd -- "$out_dir" && pwd)

is_wsl=0
if [ -r /proc/version ] && grep -qiE 'microsoft|wsl' /proc/version; then is_wsl=1; fi

# DrvFs (/mnt/<drive>) cannot host the AppDir: linuxdeploy needs real symlinks
# and preserved exec bits. Stage on the native filesystem and copy the finished
# artifact back.
stage_base=$build_dir
case "$root" in
    /mnt/*)
        if [ "$is_wsl" = "1" ]; then
            stage_base=${TMPDIR:-/tmp}/$PAYLOAD_DIR-appimage.$$
            echo "WSL: repo is on a DrvFs mount; staging AppDir at $stage_base"
        fi;;
esac
appdir=$stage_base/AppDir
tools_dir=${RECOMP_APPIMAGE_TOOLS:-"${XDG_CACHE_HOME:-$HOME/.cache}/recomp-appimage-tools"}
output=$out_dir/$EXE_NAME-$version-linux-x86_64.AppImage

cleanup() {
    [ -n "${derived_toml:-}" ] && rm -f -- "$derived_toml"
    case "$stage_base" in
        /tmp/"$PAYLOAD_DIR"-appimage.*|"${TMPDIR:-/tmp}"/"$PAYLOAD_DIR"-appimage.*)
            rm -rf -- "$stage_base";;
    esac
}
trap cleanup EXIT

if [ -z "$(ls "$root"/"$generated_dir"/*_dispatch.c 2>/dev/null)" ]; then
    echo "Missing generated game sources ($generated_dir/*_dispatch.c)." >&2
    echo "Run the recompiler first: $FRAMEWORK_DIR/recompiler/build*/psxrecomp-game --config $GAME_TOML" >&2
    exit 1
fi

# --- reproducibility anchor ------------------------------------------------
if [ -z "${SOURCE_DATE_EPOCH:-}" ]; then
    # A git worktree checked out by Windows stores an absolute Windows gitdir
    # path in .git, which WSL's git cannot follow ("not a git repository:
    # /mnt/f/...F:/..."), so this legitimately fails here. Fall back to the
    # VERSION file's mtime and say so, rather than silently stamping epoch 0.
    SOURCE_DATE_EPOCH=$(git -C "$root" log -1 --format=%ct 2>/dev/null || true)
    if [ -z "$SOURCE_DATE_EPOCH" ]; then
        SOURCE_DATE_EPOCH=$(stat -c %Y "$root/packaging/release/VERSION" 2>/dev/null || echo 0)
        echo "note: git date unavailable here; SOURCE_DATE_EPOCH from packaging/release/VERSION mtime." >&2
        echo "      Pass SOURCE_DATE_EPOCH explicitly to pin it across machines." >&2
    fi
fi
export SOURCE_DATE_EPOCH
echo "version=$version  SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH"

# --- BIOS backends ---------------------------------------------------------
# The runtime will not configure without at least one recompiled BIOS under
# psxrecomp-v4/generated. A clean checkout has none, so generate the bundled
# OpenBIOS (MIT, no dump required) and SCPH1001 when a dump is present, rather
# than failing at cmake with a message about a script we could have run.
fw=$root/$FRAMEWORK_DIR

# THE shared staging surface. Sourcing this is what makes the tag, the shard
# selection, the toolchain and the catalog framework-owned instead of forked per
# title. If the pinned submodule predates it, this fails HERE, loudly, rather
# than falling back to a local copy -- there is no local copy any more.
# shellcheck source=/dev/null
. "$fw/tools/release_overlay_stage.sh"
psx_release_stage_init "$fw"

# A CMake build directory records absolute paths and its generator's compiler,
# so a tree configured by Windows cmake (F:/..., ninja.exe) cannot be reused
# from WSL. Keep a Linux-only directory; never share build-t2 with Windows.
bios_build=${PSXRECOMP_BIOS_BUILD:-recompiler/build-linux}

# Generate a backend only when its ROM is actually present. Gating on the
# profile .toml instead would try to recompile SCPH1001 on any checkout, since
# the profile ships even when the dump does not.
bios_rom_for() {
    case "$1" in
        OpenBIOS) printf '%s\n' "$fw/bios/openbios.bin";;
        *)        printf '%s\n' "$fw/bios/$1.BIN";;
    esac
}
needed_stems=""
for stem in OpenBIOS SCPH1001; do
    [ -f "$fw/bios/$stem.toml" ] || continue
    [ -f "$(bios_rom_for "$stem")" ] || continue
    [ -f "$fw/generated/${stem}_dispatch.c" ] && continue
    needed_stems="$needed_stems $stem"
done

if [ -n "$needed_stems" ]; then
    if [ ! -x "$fw/$bios_build/psxrecomp-bios" ]; then
        gen=Ninja
        command -v ninja >/dev/null 2>&1 || gen="Unix Makefiles"
        cmake -S "$fw/recompiler" -B "$fw/$bios_build" -G "$gen" -DCMAKE_BUILD_TYPE=Release
        cmake --build "$fw/$bios_build" --target psxrecomp-bios -j "$jobs"
    fi
    for stem in $needed_stems; do
        echo "Generating recompiled BIOS backend: $stem"
        ( cd "$fw" && PSXRECOMP_BIOS_BUILD="$bios_build" \
            tools/regen_bios.sh --config "bios/$stem.toml" )
    done
fi

# --- build -----------------------------------------------------------------
if [ "$skip_build" = "0" ]; then
    generator=Ninja
    command -v ninja >/dev/null 2>&1 || generator="Unix Makefiles"
    # --build-id=none keeps the ELF a function of its sources: the default
    # build-id is a hash that also folds in link-time inputs and makes two
    # otherwise identical builds differ.
    cmake -S "$root" -B "$build_dir" -G "$generator" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER_LAUNCHER= \
        -DCMAKE_CXX_COMPILER_LAUNCHER= \
        -DPSX_SDL_BACKEND=SDL2 \
        -DPSX_DEBUG_TOOLS=OFF \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,--build-id=none"
    cmake --build "$build_dir" --target "$runtime_target" -j "$jobs"
fi

elf=$build_dir/$EXE_NAME
[ -f "$elf" ] || elf=$build_dir/psx-runtime
[ -f "$elf" ] || { echo "no runtime ELF under $build_dir" >&2; exit 1; }
file -b "$elf" | grep -q ELF || { echo "$elf is not an ELF binary" >&2; exit 1; }

# --- player game.toml ------------------------------------------------------
# Two conventions exist across the titles and both are honoured:
#   packaging/release/game.toml present -> ship it verbatim (Tomba 2: a
#       curated player config, deliberately smaller than the dev one)
#   absent -> derive from the repo's real game.toml by cutting the dev-only
#       audit block (Ape Escape: one source of truth, so the shipped config
#       cannot drift from the config that was validated)
# The codegen tag is computed from whichever file results, so the bundled
# cache always matches what the player actually runs.
player_toml=$root/packaging/release/$GAME_TOML
derived_toml=""
if [ ! -f "$player_toml" ]; then
    [ -f "$root/game.toml" ] || { echo "no packaging/release/game.toml and no game.toml" >&2; exit 1; }
    derived_toml=${TMPDIR:-/tmp}/player-game.$$.toml
    awk '
        /Audit-specific/ { exit }
        /^\[audit\]/     { exit }
        { print }
    ' "$root/game.toml" > "$derived_toml"
    # Drop a trailing run of blank/comment lines left by the cut.
    sed -i -e :a -e '/^[[:space:]]*$/{$d;N;ba' -e '}' "$derived_toml"
    player_toml=$derived_toml
    echo "player game.toml derived from game.toml (audit block stripped)"
fi

# --- release identity: game id + codegen tag -------------------------------
# The cache namespace the loader will read is <game_id>/gcc/<arch-abi>/<cg_tag>.
# Compute the tag exactly the way compile_overlays.py and the Windows packager
# do -- from the runtime includes plus the PACKAGED game.toml, so a cache built
# against the dev config (a different tag) is never mistaken for a usable one.
game_id=$(sed -n 's/^[[:space:]]*id[[:space:]]*=[[:space:]]*"\([^"]*\)".*/\1/p' \
    "$player_toml" | head -1)
[ -n "$game_id" ] || { echo "could not read [game] id from $player_toml" >&2; exit 1; }

recompiler_bin=$fw/$bios_build/psxrecomp-game
[ -x "$recompiler_bin" ] || recompiler_bin=$fw/recompiler/build-linux/psxrecomp-game
if [ ! -x "$recompiler_bin" ]; then
    recompiler_build=$(dirname -- "$recompiler_bin")
    gen=Ninja
    command -v ninja >/dev/null 2>&1 || gen="Unix Makefiles"
    cmake -S "$fw/recompiler" -B "$recompiler_build" -G "$gen" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$recompiler_build" --target psxrecomp-game -j "$jobs"
fi
# The tag comes from compile_overlays.cache_tag(), via the shared tool. What
# used to be here was a python heredoc that imported compile_overlays and then
# reformatted its tag itself, one field short -- see the header.
#
# The FLAVOR is read from what the build published
# (psxrecomp_overlay_flavor-<target>.txt, written by runtime.cmake), not
# assumed to be 0. Flavor is a property of the runtime BINARY (0 base, 2 pgxp)
# and is NOT platform-dependent; a packager that assumes 0 for a PGXP build
# stages a cache namespace the shipped binary never reads, and nothing
# anywhere fails. This is also why --skip-build against a build directory made
# by an older framework fails here: it carries no flavor publication, and
# guessing is worse than stopping.
cg_tag=$(psx_overlay_cg_tag \
    --runtime-include   "$fw/runtime/include" \
    --recompiler        "$recompiler_bin" \
    --game-toml         "$player_toml" \
    --flavor-from-build "$build_dir" \
    --runtime-target    "$runtime_target")
[ -n "$cg_tag" ] || { echo "could not compute codegen tag" >&2; exit 1; }
echo "game=$game_id  codegen tag=$cg_tag"

# --- stage AppDir ----------------------------------------------------------
rm -rf -- "$appdir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/$PAYLOAD_DIR"
payload=$appdir/usr/share/$PAYLOAD_DIR

install -m 0755 "$elf" "$appdir/usr/bin/$EXE_NAME"

# AppRun carries the version marker; stamp it rather than hardcoding.
sed -e "s|@VERSION@|$version|g" -e "s|@APP_NAME@|$APP_NAME|g" \
    -e "s|@EXE_NAME@|$EXE_NAME|g" -e "s|@PAYLOAD_DIR@|$PAYLOAD_DIR|g" \
    -e "s|@ENV_PREFIX@|$ENV_PREFIX|g" -e "s|@GAME_TOML@|$GAME_TOML|g" \
    "$root/packaging/linux/AppRun" > "$appdir/AppRun"
chmod 0755 "$appdir/AppRun"
sed -e "s|^Name=.*|Name=$APP_NAME|" \
    -e "s|^Exec=.*|Exec=$EXE_NAME|" \
    -e "s|^Icon=.*|Icon=$DESKTOP_ID|" \
    "$root/packaging/linux/io.github.mstan.Tomba2Recomp.desktop" \
    > "$appdir/$DESKTOP_ID.desktop"

for tree in assets bios; do
    [ -d "$build_dir/$tree" ] || { echo "build did not stage $tree/" >&2; exit 1; }
    cp -a "$build_dir/$tree" "$payload/$tree"
done

# --- mod catalog -----------------------------------------------------------
# mods/ is NOT part of the loop above any more. It needs three things the loop
# cannot do: drop mods/installed and mods/state.toml (this machine's own
# installed archives and its own enable/disable selection over a catalog that
# ships default-off -- both used to ship), and verify the catalog against the
# manifest the BUILD published instead of against a written-down number.
#
# The number is what makes this worth changing. This script asserted
# EXPECTED_MODS=8 for the USA variant and hard-coded 7 in the Italian branch:
# two counts of shared framework content plus game content, going stale
# independently, in a title that controls neither half. Tomba 2 has already been
# unreleasable once for exactly that -- it demanded 5 while the true catalog was
# 7, because the FRAMEWORK had gained a mod.
#
# The Italian catalog legitimately OVERRIDES framework psx.* packages with
# localized manifests at the same ids, so its id set is smaller than the sum of
# its sources. runtime.cmake deduplicates the published id list, so the derived
# check handles that with no per-variant number at all.
psx_add_mod_catalog --build-path "$build_dir" --stage "$payload" \
                    --runtime-target "$runtime_target"

# OpenBIOS must ride along with its notice; a retail BIOS must not.
[ -f "$payload/bios/openbios.bin" ] || { echo "missing bundled OpenBIOS" >&2; exit 1; }
[ -f "$payload/bios/OpenBIOS.LICENSE" ] || { echo "missing OpenBIOS notice" >&2; exit 1; }

mkdir -p "$payload/licenses"
if [ -f "$fw/runtime/licenses/libchdr-NOTICES.txt" ]; then
    cp "$fw/runtime/licenses/libchdr-NOTICES.txt" "$payload/licenses/"
fi

# --- prebuilt overlay cache + overlay toolchain ---------------------------
# Both go through the shared framework staging, which is the same code the
# Windows packager runs. Nothing about shard selection is decided here.
#
# The cache namespace the loader reads is
# cache/<game_id>/<tier>/<arch-abi>/<cg_tag>/. Linux shards are .so under
# linux-x64 and Windows are .dll under win-x64; release_stage.py takes both the
# suffix and the arch-abi from compile_overlays (overlay_ext /
# cache_arch_abi), so this script does not name either and the two platforms
# cannot disagree about them.
#
# There is no --allow-no-cache. Staging nothing is a hard failure, and the
# framework tool prints the exact compile_overlays.py invocation to build a
# cache for THIS tag.
cache_src_root=${OVERLAY_CACHE_DIR:-"$root/build-linux-cache/cache"}
psx_add_overlay_cache --game-id "$game_id" \
                      --cache-src-root "$cache_src_root" \
                      --stage "$payload" \
                      --cg-tag "$cg_tag"

# The self-contained overlay toolchain: a pinned relocatable CPython plus
# compile_overlays.py, the recompiler and the runtime headers. It is what lets a
# player whose machine has no compiler turn a newly captured overlay into native
# code instead of interpreting it forever, and NO Linux packager has ever staged
# one -- measured 2026-09-02, `grep -c overlay_toolchain` was 0 in all three
# forked copies of this file. So the shipped cache was all a Linux player ever
# got, with no way to extend it.
#
# The runtime gates this on <exe-dir>/overlay_toolchain/python/bin/python3
# existing; packaging/linux/AppRun links the payload copy into the writable data
# directory so that path resolves at runtime (the payload itself is a read-only
# squashfs mount, and the exe-dir anchor is the data directory).
psx_add_overlay_toolchain --stage "$payload" \
                          --recomp-dir "$(dirname -- "$recompiler_bin")" \
                          --recomp-tools "$fw/tools" \
                          --recomp-include "$fw/runtime/include" \
                          --dl-cache "$tools_dir" \
                          --platform linux

cp "$player_toml" "$payload/$GAME_TOML"
cp "$root/packaging/release/input.ini"      "$payload/input.ini"
cp "$root/packaging/release/START_HERE.txt" "$payload/START_HERE.txt"
cp "$root/LICENSE" "$root/README.md" "$payload/"

# recomp-ui resolves fonts/textures through SDL_GetBasePath(), which points at
# the real ELF inside the mount rather than psxrecomp's writable argv[0] anchor.
ln -s "../share/$PAYLOAD_DIR/assets" "$appdir/usr/bin/assets"

if command -v magick >/dev/null 2>&1; then image_tool=magick
elif command -v convert >/dev/null 2>&1; then image_tool=convert
else echo "ImageMagick is required for the AppImage icon." >&2; exit 1; fi
"$image_tool" "$root/$ICON_SOURCE" \
    -resize 240x240 -background transparent -gravity center -extent 256x256 \
    "$appdir/$DESKTOP_ID.png"
ln -s "$DESKTOP_ID.png" "$appdir/.DirIcon"

# --- pinned tooling --------------------------------------------------------
linuxdeploy_url=https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
linuxdeploy_sha=36a2d7e274d12e1050d0e9ecfe11d339ed54720b2bec464c286d53f8b07f5c62
appimagetool_url=https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
appimagetool_sha=a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0

mkdir -p "$tools_dir"
fetch_tool() {
    url=$1; sha=$2; dest=$3
    if [ ! -f "$dest" ] || [ "$(sha256sum "$dest" | awk '{print $1}')" != "$sha" ]; then
        curl -fL --retry 3 "$url" -o "$dest.tmp"
        printf '%s  %s\n' "$sha" "$dest.tmp" | sha256sum -c -
        mv "$dest.tmp" "$dest"
    fi
    chmod 0755 "$dest"
}
linuxdeploy=$tools_dir/linuxdeploy-x86_64.AppImage
appimagetool=$tools_dir/appimagetool-x86_64.AppImage
fetch_tool "$linuxdeploy_url" "$linuxdeploy_sha" "$linuxdeploy"
fetch_tool "$appimagetool_url" "$appimagetool_sha" "$appimagetool"

export NO_STRIP=1
"$linuxdeploy" --appimage-extract-and-run \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/$EXE_NAME" \
    --desktop-file "$appdir/$DESKTOP_ID.desktop" \
    --icon-file "$appdir/$DESKTOP_ID.png"

# Normalise mtimes so the squashfs image is byte-stable across runs.
find "$appdir" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} + 2>/dev/null || true

rm -f -- "$output"
ARCH=x86_64 "$appimagetool" --appimage-extract-and-run "$appdir" "$output"
chmod 0755 "$output"

sha256sum "$output"
echo "AppImage: $output"
