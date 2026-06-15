#!/usr/bin/env bash
#
# sync.sh — re-vendor the nOSh render substrate shim into kn86-ui
# (vendor/nosh-substrate/).
#
# WHY THIS EXISTS
# ---------------
# kn86-ui owns the Fe component kit (ui/*.lsp). Those components render
# against the nOSh render + cell API contract — a set of C primitives
# (render/*, cell-*) bound into a System-tier KEC context. To prove the kit
# green in standalone CI BEFORE the nOSh runtime exists as its own repo, we
# vendor a FROZEN copy of the minimal C substrate the golden-buffer harness
# needs to link headless (no SDL3):
#
#     sys_render.c / sys_render.h   binds the render/* primitives
#     sys_context.c / sys_context.h System-tier KEC context lifecycle
#     render.c / render.h           in-memory RGB565 render surface
#     phosphor.c / phosphor.h       phosphor scheme (EMBER golden values)
#     font.c / font.h               8x8 KN-86 Code Page glyph bitmaps
#     cell_api.c / cell_api.h       the cell-* primitive surface
#     render.lsp                    System-tier Fe wrapper lib the harness loads
#     kec-lisp/                     the vendored Fe VM (own sync.sh)
#
# This substrate is BUILD SCAFFOLDING, not a kn86-ui deliverable. The
# deliverable is ui/*.lsp.
#
# SOURCE OF TRUTH (today, P1): the kn86-deckline monorepo
# (git@github.com:jschairb/kn86-deckline.git), checked out at
# ~/src/kinoshita. The C files come from kn86-emulator/src/, render.lsp from
# kn86-emulator/system-image/lib/.
#
# Vendored at kn86-deckline commit:
#
#     0fea6e893e808c9415a6320b8fc954355f164b59   (branch: main)
#
# SOURCE FLIPS IN P4: when the nOSh runtime is extracted to its own repo,
# this shim's source of truth becomes that nOSh repo. Update SRC and the
# recorded commit then; the file list above should not change.
#
# To re-sync (P1, from the monorepo):
#
#     KN86_DECKLINE_SRC=~/src/kinoshita ./vendor/nosh-substrate/sync.sh
#
# then update the recorded commit hash above. Re-vendor the Fe VM
# separately via ./vendor/nosh-substrate/kec-lisp/sync.sh.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${KN86_DECKLINE_SRC:-$HERE/../../../kinoshita}"
EMU="$SRC/kn86-emulator"

if [ ! -d "$EMU/src" ]; then
    echo "error: kn86-deckline source not found at $SRC" >&2
    echo "       set KN86_DECKLINE_SRC to the monorepo checkout path." >&2
    exit 1
fi

for f in sys_render sys_context render phosphor font cell_api; do
    cp "$EMU/src/$f.c" "$HERE/$f.c"
    cp "$EMU/src/$f.h" "$HERE/$f.h"
done
cp "$EMU/system-image/lib/render.lsp" "$HERE/render.lsp"

REV="$(git -C "$SRC" rev-parse HEAD 2>/dev/null || echo unknown)"
echo "Re-vendored nOSh render substrate @ $REV"
echo "Remember to update the recorded commit hash in this script."
echo "The Fe VM is vendored separately: ./vendor/nosh-substrate/kec-lisp/sync.sh"
