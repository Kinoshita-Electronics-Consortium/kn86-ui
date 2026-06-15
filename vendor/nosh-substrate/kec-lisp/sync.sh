#!/usr/bin/env bash
#
# sync.sh — re-vendor kec-lisp into kn86-ui's substrate shim
# (vendor/nosh-substrate/kec-lisp/).
#
# kec-lisp is a SEPARATE GitHub repo
# (Kinoshita-Electronics-Consortium/kec-lisp), checked out as a sibling at
# ~/src/kinoshita/kec-lisp. We vendor a frozen copy of its language core so
# kn86-ui's standalone CI can build the Fe component kit without depending on
# a sibling checkout.
#
# Vendored at kec-lisp commit:
#
#     8a7b248a9deea01682604d4b3e887d45333af32e   (branch: main)
#
# This is the SAME kec stack the kn86-deckline monorepo vendors; it carries
# kec_open_with_arena (no-malloc arena embedding), the dependency the
# System-context render substrate gates on.
#
# Files vendored (READ-only mirror — never edit the copies here; edit
# upstream and re-run this script):
#
#     kernel/{fe.c,fe.h}     Layer 0  — vendored Fe VM (KEC variant).
#     core/*.lsp             Layer 1  — KEC Core prelude (embedded at build).
#     host/{host.c,host.h}   Layer 2  — portable host stdlib.
#     runtime/{kec.c,kec.h}  Layer 2  — embedding API + Core injection.
#     tools/mkembed.c        codegen  — bakes core/*.lsp into a C string literal.
#
# To re-sync to the current kec-lisp main:
#
#     ./vendor/nosh-substrate/kec-lisp/sync.sh
#
# then update the recorded commit hash above.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${KEC_LISP_SRC:-$HERE/../../../../kec-lisp}"

if [ ! -d "$SRC/kernel" ]; then
    echo "error: kec-lisp source not found at $SRC" >&2
    echo "       set KEC_LISP_SRC to the kec-lisp checkout path." >&2
    exit 1
fi

mkdir -p "$HERE/kernel" "$HERE/core" "$HERE/host" "$HERE/runtime" "$HERE/tools"
cp "$SRC/kernel/fe.c" "$SRC/kernel/fe.h"     "$HERE/kernel/"
cp "$SRC"/core/*.lsp                          "$HERE/core/"
cp "$SRC/host/host.c" "$SRC/host/host.h"     "$HERE/host/"
cp "$SRC/runtime/kec.c" "$SRC/runtime/kec.h" "$HERE/runtime/"
cp "$SRC/tools/mkembed.c"                     "$HERE/tools/"

REV="$(git -C "$SRC" rev-parse HEAD 2>/dev/null || echo unknown)"
echo "Re-vendored kec-lisp @ $REV"
echo "Remember to update the recorded commit hash in this script."
