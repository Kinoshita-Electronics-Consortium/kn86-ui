# kn86-ui

The KN-86 Deckline **Fe component kit** — a library of reusable UI
components written in [KEC Lisp](https://github.com/Kinoshita-Electronics-Consortium/kec-lisp)
for the KN-86 handheld terminal.

This repo is the **source of truth** for the `ui/` kit. It is vendored into
its consumers (the nOSh runtime, the kn86-sdk, and kn86-carts) via each
consumer's own `sync.sh` — the same pattern kec-lisp uses. Edit the
components here; re-vendor downstream.

The components render against nOSh's **render + cell API contract**: a set
of privileged `render/*` and `cell-*` primitives bound into a System-tier
KEC context. They draw to an 8×8 KN-86 Code Page glyph grid on the
1024×600 RGB565 surface (the 128×75 cell ceiling). None of the device
hardware is here — just the components and a frozen substrate to test them.

## Components

| File | What it draws |
|------|---------------|
| `ui/glyphs.lsp` | Glyph + colour constants every other file references. |
| `ui/frames.lsp` | Frames + chrome rows (status bar / action bar), headline, status-glyph. |
| `ui/compositing.lsp` | Depth cues (scrim, shadow), the selection primitive, and composed overlays (modal, popup, palette). |
| `ui/lists.lsp` | Data-navigation widgets — lists, trees, tables. |
| `ui/data.lsp` | Data-display primitives — vertical-bar sparkline + horizontal fine-bar (the magnitude channel). |
| `ui/input.lsp` | Navigation + input widgets plus transition wrappers. |

Load order is `glyphs` first (constants), then the rest. The consumer's
loader (and the test harness) loads the `render/` wrapper lib before any
`ui/` file.

## Build

Needs CMake and a C compiler. No SDL — the component tests run headless.

```sh
cmake -S . -B build
cmake --build build
(cd build && ctest --output-on-failure)
```

Three test executables (`test_ui_frames`, `test_ui_lists_data`,
`test_ui_input`) load `render/` + `ui/` into a System-tier KEC context,
eval a component form, and assert the in-memory render surface
glyph-by-glyph (a golden-buffer harness, `tests/ui_golden.h`).

## How the standalone build works

The components can't run without the nOSh render substrate (the C that
binds `render/*` + `cell-*` and the Fe VM they evaluate on). Until the nOSh
runtime is its own repo, `vendor/nosh-substrate/` carries a **frozen copy**
of the minimal substrate the tests need:

```
vendor/nosh-substrate/
  sys_render.{c,h}    binds the render/* primitives
  sys_context.{c,h}   System-tier KEC context lifecycle
  render.{c,h}        in-memory RGB565 render surface
  phosphor.{c,h}      phosphor scheme (EMBER golden values)
  font.{c,h}          8×8 KN-86 Code Page glyph bitmaps
  cell_api.{c,h}      the cell-* primitive surface
  render.lsp          the System-tier Fe wrapper lib the harness loads
  kec-lisp/           the vendored Fe VM (has its own sync.sh)
  sync.sh             re-vendors the substrate; records the source commit
```

This substrate is **build scaffolding, not a deliverable** — the
deliverable is `ui/*.lsp`. Its source of truth is the kn86-deckline
monorepo today; it flips to the standalone nOSh repo in P4. See
`vendor/nosh-substrate/sync.sh`.

## License

MIT. The vendored Fe kernel is from [rxi/fe](https://github.com/rxi/fe)
(also MIT). See [LICENSE](LICENSE).
