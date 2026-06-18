# CLAUDE.md

Agent guidance for the **kn86-ui** repo. This is the authoritative per-repo agent
doc; `AGENTS.md` is a thin pointer to it.

## What this is

**kn86-ui is the KN-86 Deckline Fe component kit** — a library of reusable UI
components for the terminal UI, authored in
[KEC Lisp](https://github.com/Kinoshita-Electronics-Consortium/kec-lisp)
(the embedded Fe-Lisp dialect). The deliverable is `ui/*.lsp` and nothing else.

The kit covers six concerns, one file each:

| File | What it draws |
|------|---------------|
| `ui/glyphs.lsp` | Glyph + colour constants every other file references. Load first. |
| `ui/frames.lsp` | Frames + chrome rows (status bar / action bar), headline, status-glyph. |
| `ui/compositing.lsp` | Depth cues (scrim, shadow), the selection primitive, composed overlays (modal, popup, palette). |
| `ui/lists.lsp` | Data-navigation widgets — lists, trees, tables. |
| `ui/data.lsp` | Data-display primitives — vertical-bar sparkline + horizontal fine-bar (the magnitude channel). |
| `ui/input.lsp` | Navigation + input widgets plus transition wrappers. |

The components are **pure Fe-Lisp**. They render through nOSh's privileged
`render/*` and `cell-*` primitives, which exist only in a System-tier KEC
context (loading the kit into a cart context raises "unbound symbol" by
design — that is the capability boundary working). They are tested through a
small C harness that **vendors the render substrate** (see Layout) so the kit
can prove itself green headless, without the full device stack.

## System context

kn86-ui is one repo in the KN-86 multi-repo ecosystem
(Kinoshita-Electronics-Consortium). For the authoritative ecosystem map —
which repo owns what and how they fit together — see:

- The **`jschairb/kn86-deckline`** monorepo `CLAUDE.md` → "Repository Topology".
- **`kn86-docs` ADR-0039** (the multi-repo split decision).

Do not restate that map here; link to it.

This repo's place in it:

- It **vendors a frozen render-substrate shim** (plus a vendored `kec-lisp`) under
  `vendor/nosh-substrate/` purely so standalone CI can build and test the
  components before the nOSh runtime exists as its own repo. The shim is build
  scaffolding, **not** a deliverable.
- It is **consumed by nOSh** (system screens), **kn86-sdk**, and **kn86-carts**,
  which vendor `ui/*.lsp` in via their own `sync.sh` (the same pattern kec-lisp
  uses). Edit components here; re-vendor downstream.
- It targets **nOSh's render + cell-API contract** — the `render/*` and `cell-*`
  primitive surface. The components are written against that contract, not
  against any hardware directly.

**The UI design language is described in `kn86-docs`** (`ui-design-language.md`).
Component headers cite it by section for context (§4 four-channel rule, §6 frames,
§8 compositing, §10 component kit). Those docs are a record of what we built — when
you change a component, change the code first, then update the docs to match. Docs
never gate or block a change.

**Hardware values** (display, grid, glyph set, colour, phosphor schemes) are listed
for reference in `kn86-docs` and the kn86-deckline monorepo `CLAUDE.md`. They follow
the build; if a value here and one there disagree, the working code wins — fix
whichever is stale, no sign-off needed.

## Build / test

Needs CMake and a C compiler. No SDL — the component tests run headless.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

CI (`.github/workflows/ci.yml`) runs exactly this matrix on `ubuntu-latest` and
`macos-latest`.

There are three test executables (`test_ui_frames`, `test_ui_lists_data`,
`test_ui_input`). Each loads `render.lsp` + the `ui/` files into a System-tier
KEC context, evals a component form, and asserts the in-memory render surface
**glyph-by-glyph** — a golden-buffer harness (`tests/ui_golden.h`). When you add
or change a component, add or update the golden assertions for it; the harness,
not the device, is the acceptance gate here.

## Layout

```
ui/                     THE DELIVERABLE — the Fe component kit (*.lsp)
tests/
  ui_golden.h           golden-buffer harness (loads render/ + ui/, reads the surface)
  kn86_test.h           tiny test macros (KN86_TEST / KN86_ASSERT / KN86_REPORT)
  test_ui_frames.c      frames + chrome + compositing
  test_ui_lists_data.c  lists + data
  test_ui_input.c       nav + input
vendor/nosh-substrate/  FROZEN render-substrate shim (build scaffolding, NOT a deliverable)
  sys_render.{c,h}      binds the render/* primitives
  sys_context.{c,h}     System-tier KEC context lifecycle
  render.{c,h}          in-memory RGB565 render surface
  phosphor.{c,h}        phosphor scheme (AMBER golden values)
  font.{c,h}            8x8 KN-86 Code Page glyph bitmaps
  cell_api.{c,h}        the cell-* primitive surface
  render.lsp            System-tier Fe wrapper lib the harness loads
  kec-lisp/             vendored Fe VM (has its own sync.sh)
  sync.sh               re-vendors the substrate; records the source commit
CMakeLists.txt          builds the embed tool, stages the lib tree, defines the tests
```

**`vendor/` is vendored, not authored.** Do not hand-edit files under
`vendor/nosh-substrate/` (including `kec-lisp/`) to fix a component problem — they
are a frozen copy of upstream. Re-sync them via the appropriate `sync.sh` and
update the recorded source commit. Substrate source is the kn86-deckline monorepo
today; it flips to the standalone nOSh repo in P4 (see the `sync.sh` header).

## Conventions

**KEC-Lisp component style** (match the existing files — read a couple before
writing):

- One file per concern; `ui/glyphs.lsp` (constants) loads first, then the rest in
  dependency order. The header comment in `ui/glyphs.lsp` documents the load
  order — keep it accurate.
- **Namespace every public symbol `ui/…`** (`ui/panel`, `ui/list-row`,
  `ui/select-row`). Define functions with `defn`.
- **Name glyph + colour codes once** as `UI-…` `define` constants in
  `ui/glyphs.lsp`; reference them by name in component sources so the code reads
  as intent, never as a magic number. Cross-check codes against the canonical
  character set and `vendor/nosh-substrate/font.c`.
- Author in **KEC Core vocabulary** (`defn` / `let` / `cond` / `when` / `do` /
  `dolist` / `dotimes` / `map` / `fold-left` / `floor` / `str` …). KEC uses `set`
  for assignment and `=` / `is` for equality (the kec-lisp migration) — **not**
  the legacy `setq`/`eq`. See `vendor/nosh-substrate/kec-lisp/core/*.lsp`.
- Comments are `;` line comments. Lead each public function with a comment giving
  its signature and a one-line contract (the `(ui/foo a b) -> nil` form), and
  cite the governing `ui-design-language.md` section.
- **Coordinate convention** (`sys_render.h`): `render/box` + `render/glyph` take
  CELL coords; `render/text` + `render/fill-rect` take PIXEL coords. Components are
  cell-addressed and convert (`* cell 8`) at the pixel seam.
- **The §4 four-channel design intent** (how the kit currently works): identity = glyph,
  magnitude = density, selection = **inversion** (a black-on-phosphor colour pair,
  not a second colour), attention = border weight / leading-glyph column / blink.
  One foreground hue throughout. Change it if you want a different look — this just
  describes what's there now.

**Git / PR workflow:** branch off freshly-fetched `main`; conventional-commit
messages; open a PR rather than pushing to `main`. CI must be green (both OS legs)
before merge.
