#!/usr/bin/env bash
# test_appimage_layout.sh — verify a built recomp AppImage seeds the
# writable data directory correctly, without launching the game.
#
# Runs the AppImage with <ENV_PREFIX>_SEED_ONLY=1, which makes AppRun perform
# its seeding and print the data directory instead of exec'ing the runtime.
# Then asserts the layout the release promises.
#
# Usage: bash tools/test_appimage_layout.sh [--variant usa|ita] [--version <ver>] path/to/<Title>-<ver>-linux-x86_64.AppImage
#
# There is deliberately no --allow-no-cache. A release AppImage that seeds no
# overlay shards is a release in which every player's first visit to every area
# runs on the dirty-RAM interpreter, and that relaxation is exactly what let a
# packager staging ZERO shards pass for a successful build (bead
# beads-eio.3.102).
set -euo pipefail

variant=usa
expected_version=""
while [ $# -gt 0 ]; do
    case "$1" in
        --variant) variant=${2:-}; shift 2;;
        --version) expected_version=${2:-}; shift 2;;
        *) break;;
    esac
done

appimage=${1:-}
[ -n "$appimage" ] || { echo "usage: $0 <AppImage>" >&2; exit 2; }
[ -x "$appimage" ] || { echo "not executable: $appimage" >&2; exit 1; }

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
[ -n "$expected_version" ] || expected_version=$(tr -d ' \t\r\n' < "$root/packaging/release/VERSION")
# Per-game identity: ENV_PREFIX names the AppImage's env overrides. The size of
# the catalog is NOT written down here -- see the derived check below.
# shellcheck source=/dev/null
. "$root/packaging/release/app.conf"
GAME_TOML=${GAME_TOML:-game.toml}
case "$variant" in
    usa) ;;
    ita)
        ENV_PREFIX="TOMBI2_RECOMP_ITA"
        GAME_TOML="game_ita.toml"
        ;;
    *) echo "unknown variant: $variant" >&2; exit 2;;
esac

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

data_dir=$(env "${ENV_PREFIX}_DATA_DIR=$work/data" "${ENV_PREFIX}_SEED_ONLY=1" \
    "$appimage" --appimage-extract-and-run)
[ -n "$data_dir" ] || { echo "AppRun printed no data dir" >&2; exit 1; }

fail=0
check_file() { [ -f "$data_dir/$1" ] || { echo "MISSING file: $1" >&2; fail=1; }; }
check_dir()  { [ -d "$data_dir/$1" ] || { echo "MISSING dir:  $1" >&2; fail=1; }; }

for d in saves cache mods assets bios; do check_dir "$d"; done
for f in "$GAME_TOML" input.ini START_HERE.txt LICENSE README.md \
         bios/openbios.bin bios/OpenBIOS.LICENSE .appimage-layout-version; do
    check_file "$f"
done

got_version=$(tr -d ' \t\r\n' < "$data_dir/.appimage-layout-version")
if [ "$got_version" != "$expected_version" ]; then
    echo "version marker mismatch: AppImage says '$got_version', VERSION says '$expected_version'" >&2
    fail=1
fi

# Mod catalog, checked as an INVARIANT rather than against a number. This used
# to assert EXPECTED_MODS, a count of shared framework packages plus game
# packages that no title controls both halves of; it went stale the first time
# the framework added a mod, and the Italian variant carried a second copy of
# it. The packager already verifies the staged catalog against the manifest the
# BUILD published, so what is left to check here is that seeding preserved it:
# every package directory that arrived must carry a manifest, and the catalog
# must not be empty.
if [ ! -d "$data_dir/mods/bundled" ]; then
    echo "MISSING dir:  mods/bundled (the shipped catalog lives there)" >&2
    fail=1
else
    pkg_dirs=$(find "$data_dir/mods/bundled" -mindepth 1 -maxdepth 1 -type d | wc -l)
    manifests=$(find "$data_dir/mods/bundled" -name manifest.toml | wc -l)
    if [ "$pkg_dirs" -eq 0 ]; then
        echo "seeded mod catalog is empty; an empty Mods page must not ship" >&2
        fail=1
    elif [ "$manifests" -lt "$pkg_dirs" ]; then
        echo "seeded catalog has $pkg_dirs package dir(s) but only $manifests manifest.toml" >&2
        fail=1
    fi
    echo "seeded mod catalog: $pkg_dirs package(s), $manifests manifest(s)"
fi

# The bundled overlay cache must arrive as Linux .so shards under
# gcc/linux-x64; a .dll here would mean the Windows cache was staged and the
# loader (which dlopen()s OVERLAY_SHARED_EXT) would ignore every one of them.
seeded_so=$(find "$data_dir/cache" -name '*.so' 2>/dev/null | wc -l)
if [ "$seeded_so" -eq 0 ]; then
    echo "seeded cache holds no .so shards" >&2
    fail=1
fi
# Count rather than pipe into `grep -q`: grep exits on its first match and
# SIGPIPEs find, and under `set -o pipefail` that makes the whole pipeline
# report failure -- so a NEGATED grep -q test fires precisely when the thing
# it is looking for IS present. That false alarm cost a debug cycle here.
stray_dll=$(find "$data_dir/cache" -name '*.dll' | wc -l)
if [ "$stray_dll" -ne 0 ]; then
    echo "seeded cache contains $stray_dll Windows .dll shards; the Linux loader cannot use them" >&2
    fail=1
fi
arch_so=$(find "$data_dir/cache" -path '*/linux-x64/*' -name '*.so' | wc -l)
if [ "$seeded_so" -gt 0 ] && [ "$arch_so" -eq 0 ]; then
    echo "cache .so shards are not under a linux-x64 arch-abi directory" >&2
    fail=1
fi

# A retail BIOS or disc image must never be inside the payload.
stray=$(find "$data_dir" \( -iname 'SCPH*.BIN' -o -iname '*.cue' -o -iname '*.iso' \
        -o -iname '*.mcd' \) -print 2>/dev/null || true)
if [ -n "$stray" ]; then
    echo "payload contains files that must never ship:" >&2
    printf '  %s\n' $stray >&2
    fail=1
fi

# Seeding must be idempotent: a second run must not fail or duplicate.
env "${ENV_PREFIX}_DATA_DIR=$work/data" "${ENV_PREFIX}_SEED_ONLY=1" \
    "$appimage" --appimage-extract-and-run >/dev/null

# A shard the player's own session built must survive a reseed: AppRun seeds
# the cache with cp -n, so release shards fill gaps without overwriting.
user_shard=$data_dir/cache/.user-shard-probe
printf 'player-built\n' > "$user_shard"
env "${ENV_PREFIX}_DATA_DIR=$work/data" "${ENV_PREFIX}_SEED_ONLY=1" \
    "$appimage" --appimage-extract-and-run >/dev/null
if [ ! -f "$user_shard" ] || [ "$(cat "$user_shard")" != "player-built" ]; then
    echo "reseed destroyed a player-built cache entry" >&2
    fail=1
fi

# User-owned files must survive a reseed.
echo "; user edit" >> "$data_dir/input.ini"
before=$(sha256sum "$data_dir/input.ini" | awk '{print $1}')
env "${ENV_PREFIX}_DATA_DIR=$work/data" "${ENV_PREFIX}_SEED_ONLY=1" \
    "$appimage" --appimage-extract-and-run >/dev/null
after=$(sha256sum "$data_dir/input.ini" | awk '{print $1}')
if [ "$before" != "$after" ]; then
    echo "reseed clobbered user-owned input.ini" >&2
    fail=1
fi

# Shards must live under exactly ONE codegen tag directory. More than one means
# the packager shipped a foreign namespace the runtime will ignore (download
# weight for nothing); zero with shards present would mean the layout is wrong.
tag_dirs=$(find "$data_dir/cache" -type d -name 'cg*' | wc -l)
if [ "$seeded_so" -gt 0 ] && [ "$tag_dirs" -ne 1 ]; then
    echo "seeded cache has $tag_dirs codegen tag directories; exactly 1 is shippable" >&2
    find "$data_dir/cache" -type d -name 'cg*' >&2
    fail=1
fi

# Producer-side artifacts that must never ship. .abi_<tag>.ok is the worst of
# the three: it is a completed-sweep memo, so shipping it tells a player's first
# launch that an ABI sweep it never ran already passed, and a stale shard gets
# loaded instead of rejected.
for pat in '.abi_*.ok' '*.c' '*.pair-lock'; do
    n=$(find "$data_dir/cache" -name "$pat" 2>/dev/null | wc -l)
    if [ "$n" -ne 0 ]; then
        echo "seeded cache contains $n '$pat' file(s), which must never ship" >&2
        fail=1
    fi
done

# The overlay toolchain gate. The runtime looks for exactly this path relative
# to the running executable, and <exe-dir> is the data directory; if it is not
# here then autocompile is off and any overlay outside the shipped cache is
# interpreted forever. No Linux packager staged one before beads-eio.3.102, so
# this assertion is the difference between the feature existing and not.
tk_py=$data_dir/overlay_toolchain/python/bin/python3
if [ ! -x "$tk_py" ]; then
    echo "MISSING overlay toolchain interpreter: overlay_toolchain/python/bin/python3" >&2
    fail=1
elif ! "$tk_py" -c 'import sys; sys.exit(0)' 2>/dev/null; then
    echo "bundled overlay toolchain interpreter does not run: $tk_py" >&2
    fail=1
else
    echo "overlay toolchain: $("$tk_py" -V 2>&1) at overlay_toolchain/python/bin/python3"
fi
if [ ! -f "$data_dir/overlay_toolchain/compile_overlays.py" ]; then
    echo "MISSING file: overlay_toolchain/compile_overlays.py" >&2
    fail=1
fi
if [ ! -x "$data_dir/overlay_toolchain/psxrecomp-game" ]; then
    echo "MISSING file: overlay_toolchain/psxrecomp-game" >&2
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    echo "AppImage layout test FAILED" >&2
    exit 1
fi
echo "AppImage layout test passed ($expected_version)"
