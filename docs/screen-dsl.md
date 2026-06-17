---
title: Screen DSL
description: The kn86-ui screen render engine — a declarative screen tree the engine flows and paints, browser-style.
---

A **screen is data** — a declarative tree of nodes. `(ui/draw tree)` lays it
out and paints it: tree = DOM, the engine = layout + paint. The engine owns
layout (block flow + an absolute escape hatch) and paints through the
`render/*` tier, reusing the [component kit](/kn86-ui/component-kit/) as its
draw vocabulary. It implements the
[UI Design Language §11 screen schema](/kn86-ui/ui-design-language/#11-screen-spec-schema-feeds-the-workbench).

## Node form

```
(tag (attrs...) child...)
```

`attrs` is a flat symbol-keyed plist, possibly `()` — e.g. `(scale 5 align center)`.
Children are nodes, or a leaf string (text content).

## Example

```lisp
(ui/draw
 '(screen ()
    (status-bar (handle "ROOK" cr 2480 sec "MISSIONS"))
    (stack (gap 1)
      (headline  (scale 5 align center) "ICE BREAKER")
      (subtitle  (scale 2 align center) "INTRUSION     STANDARD LEVEL")
      (rule ())
      (text () "The signal drops at 0300. Bring the cutter and do not")
      (text () "trip the ICE. Three gates, one timing window, no retries."))
    (at (x 700 y 216 scale 4) "03:00")
    (action-bar () ("EVAL" "accept") ("CDR" "next") ("BACK" "exit"))))
```

## Tags (Phase 1)

| Tag | Attrs | Notes |
|-----|-------|-------|
| `screen` | — | Root. Pins `status-bar` to Row 0, `action-bar` to the last row; flows the rest down the body. |
| `status-bar` | `handle cr sec` | Inverted Row-0 chrome (scale 2). |
| `action-bar` | — (children `(key label)`) | Inverted last-row chrome (scale 2). |
| `stack` | `gap pad` | Vertical flow of children (cells). |
| `text` | `scale align dim` | Body (default scale 1). `align center` centers on the surface. |
| `subtitle` | `scale align` | Default scale 2. |
| `headline` | `scale align` | Default scale 5. |
| `rule` | `w` | Horizontal divider. |
| `spacer` | `h` | Blank vertical space (cells). |
| `at` | `x y scale` | **Absolute** escape hatch (pixels). Draws and does not advance flow. |

Phase 2 adds `row`, `panel`, `list`, `table`, `metric`, `sparkline`, `clock`,
and trailing-keyword attr sugar; Phase 3 adds a `cell-*` paint backend so
carts render through the same engine.

## Layout + styling

Layout is in **glyph units** (1 unit = an 8 px cell) with per-element integer
`:scale` (mixed scale composes on one screen — body at 1×, a headline at 5×, a
clock at 4×). Styling lives in small functions in `ui/screen.lsp` — e.g. chrome
padding is `(defn scr/chrome-vpad () 2)`. The engine is **grid-parametric**:
metrics come from the surface, so the same tree renders on whatever grid the
active display profile selects (it does not bake 128×75 vs 80×25).

## Previewing a screen

`tools/render_screen` renders a screen file to the **real** RGB565 surface
(same C renderer + `font.c` the panel uses) and dumps a PPM:

```sh
cmake --build build --target render_screen
./build/tools/render_screen examples/ice-breaker.lsp out.ppm
```

The `preview/` server wraps this in a live web viewer (screen list · real
render · Lisp editor) that re-renders on save and on file change. See
`preview/README` (or run `node preview/server.mjs`).
