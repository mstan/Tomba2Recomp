#!/usr/bin/env bash
# test_no_local_cache_tag.sh — this repository must not format the overlay
# cache tag itself, and its packagers must route through the framework's shared
# release staging.
#
# The CHECK is not implemented here. It is the framework's
# runtime/tests/test_packagers_never_format_cache_tag.py, invoked against this
# tree with --root. That is the point: a rule about not duplicating logic must
# not itself be duplicated into five title repositories, which is precisely how
# the bug it guards against reached three of them.
#
# WHAT IT GUARDS (bead beads-eio.3.102)
# -------------------------------------
# tools/package_appimage.sh used to derive the shard cache tag in a python
# heredoc that imported compile_overlays.py — the module that owns the tag —
# and then reformatted the string itself. When the framework appended an
# `_f<flavor>` field, ApeEscapeRecomp's fork of this same script was
# hand-patched to append it and this one was not. The shard filter
# `find -path "*/$cg_tag/*"` needs a separator immediately after the tag and
# the real directory is `..._f0`, so it matched nothing: a cache holding
# hundreds of valid shards produced shards=0 and this packager exited 1.
# Tomba 2 v0.0.9 therefore shipped Windows-only — no AppImage asset at all.
#
# Usage: bash tools/test_no_local_cache_tag.sh
set -euo pipefail

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
app_conf=$root/packaging/release/app.conf
# shellcheck source=/dev/null
. "$app_conf"
fw=$root/${FRAMEWORK_DIR:?app.conf does not set FRAMEWORK_DIR}
check=$fw/runtime/tests/test_packagers_never_format_cache_tag.py

if [ ! -f "$check" ]; then
    echo "$check is missing." >&2
    echo "  The pinned framework predates the shared check (beads-eio.3.102);" >&2
    echo "  bump the $FRAMEWORK_DIR submodule." >&2
    exit 1
fi

# `python3`, never a bare `python`: a bare `python` can bind to a Cygwin build
# that SIGSEGVs when spawned from a job.
py=${PSX_RELEASE_STAGE_PYTHON:-python3}
command -v "$py" >/dev/null 2>&1 || { echo "no '$py' on PATH" >&2; exit 1; }

exec "$py" "$check" --root "$root"
