---
title: "Screen render engine — design"
description: Approved design for the kn86-ui screen render engine (declarative tree + layout + pluggable paint backend).
---

# kn86-ui as a screen render engine — design

*2026-06-16. Approved in brainstorming session.*

## Goal

Turn `kn86-ui` from an imperative draw-call kit into the **default screen
render engine** for the KN-86 Deckline: a KEC Lisp library other projects
(nOSh system UI, carts) call at runtime to **draw a declarative screen tree**
— browser-style (tree = DOM, this = layout + paint engine).

## Decisions (from the session)

1. **A screen is data** — a declarative tree of nodes, inspectable/portable.
   `(ui/draw tree)` builds it (paints the surface).
2. **The engine owns layout** — block flow (`stack` / `row` + gaps, intrinsic
   sizing) plus an `at` absolute escape hatch. No flexbox.
3. **It renders** (not just emits) — a runtime engine, the shared default kit.
4. **Pluggable paint backend** — the tree + layout are tier-agnostic; paint
   goes through a thin *surface seam*. Bind it to `render/*` (system) now; add
   a `cell-*` binding (carts) later, no rework.
5. **Grid-parametric** — the engine does NOT bake 128×75 or 80×25. Layout is
   in glyph units (1 unit = an 8px cell) with per-element integer `:scale`;
   the active display profile decides physical metrics. The same tree renders
   on any grid. (Resolves the CLAUDE.md-vs-code grid contradiction:
   `[[kn86-display-grid-contradiction]]`.)

## Architecture

```
screen tree (Lisp data)  →  layout pass (pure Lisp)  →  surface seam  →  render/*  (system, now)
   ui/screen.lsp             assigns px + scale          surf/*          cell-*    (carts, later)
```

- Lives in `kn86-ui/ui/screen.lsp`, reusing `frames/lists/data/compositing`
  as paint vocabulary. No C rewrite.
- The seam is `surf/text`, `surf/fill` (+ helpers) over the existing
  pixel-addressed, scale-aware `render/text` / `render/fill-rect`.

## Node vocabulary

Node form (Phase 1 canonical): `(tag (attrs…) child…)` where `attrs` is a
flat symbol-keyed plist (may be `()`). Pretty trailing-keyword sugar is a
later reader enhancement.

- **Frame:** `screen`, `status-bar`, `action-bar` (auto-pinned row 0 / last)
- **Layout:** `stack` (`:gap :pad`), `row` (`:gap`), `at`, `spacer`
- **Content:** `headline` / `subtitle` / `text` (`:scale :align`), `rule`,
  `list` / `row` / `table`, `metric` / `sparkline` / `fine-bar`,
  `status-glyph`, `clock`

## Preview path (real renderer → PNG)

`tools/render_screen` (C, built on `tests/ui_golden.h`): open a System KEC
context, load `render/` + `ui/` + `ui/screen.lsp` + a screen file, then dump
`render_surface()` (RGB565) to a PPM → PNG. **Same C renderer + `font.c` the
panel uses** — the PNG is pixel-identical to `/dev/fb0`. Golden-buffer tests
assert screens glyph-by-glyph.

## Build order

- **Phase 1 (this slice):** `screen / stack / text / headline / subtitle /
  rule / status-bar / action-bar` + `render/*` seam + `render_screen` PNG tool
  → render a real screen from a DSL tree, confirm the PNG.
- **Phase 2:** `row / panel / list / table / metric / sparkline / fine-bar /
  at / clock`, attr sugar.
- **Phase 3:** `cell-*` backend (carts); display-profile metrics wired in.

## Out of scope (YAGNI for now)

Non-square cells (12×24 "ADR-0014 look" needs a non-square glyph scale —
deferred), animation/transitions, input handling (the engine draws; input
stays in `ui/input.lsp`).
