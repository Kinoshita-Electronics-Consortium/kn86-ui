---
title: Component Kit
description: The kn86-ui component library — frames, lists, data, compositing, glyphs, input — and the render contract they draw against.
---

The component kit is a set of KEC Lisp files under `ui/`, the source of truth
vendored into nOSh / the SDK / the carts. Every component obeys the
[four-channel single-color rule](/kn86-ui/ui-design-language/#4-the-single-color-discipline-the-four-channel-rule):
identity = glyph, selection = inversion, magnitude = density, attention =
border-weight / leading-glyph / blink. There is never a second color.

## Render contract

Components draw through the privileged system-tier `render/*` primitives over
a **1024×600 RGB565** surface. Glyphs are the **8×8 KN-86 Code Page**
([Character Set](/kn86-ui/character-set/)) drawn at integer scale — a glyph at
scale `s` is `8s × 8s` px. Coordinates: `render/text` + `render/fill-rect`
take **pixels**; `render/glyph` + `render/box` take **cells**. The default
phosphor is EMBER `#D84810`.

## Files

| File | Components |
|------|-----------|
| `ui/glyphs.lsp` | Glyph + color constants every other file references. |
| `ui/frames.lsp` | `cbox`, `panel`/`box`, `rule`, `status-bar`, `action-bar`, `headline`, `status-glyph`. |
| `ui/lists.lsp` | `list-row`, `list`, `tree`, `table` (pinned header, typed-column glyphs). |
| `ui/data.lsp` | `sparkline` (vbar ramp), `fine-bar` (sub-cell horizontal bar). |
| `ui/compositing.lsp` | `scrim`, `shadow`, `select-row`, `modal`, `popup`, `palette`. |
| `ui/input.lsp` | Navigation + input widgets + transition wrappers. |
| `ui/screen.lsp` | The screen render engine — see [Screen DSL](/kn86-ui/screen-dsl/). |

Load order is `glyphs` first (constants), then the rest; the consumer's
loader loads the `render/` wrapper lib before any `ui/` file.

## Selected signatures

```lisp
;; frames
(ui/cbox cx cy cw ch title weight)     ; weight 'single | 'double; title cutout
(ui/panel cx cy cw ch title)           ; single-weight cbox
(ui/rule cx cy cw)                      ; horizontal divider
(ui/status-bar handle credits header)  ; Row 0 chrome, inverted
(ui/action-bar bindings)               ; Row 74 chrome; bindings = ((key . label) ...)
(ui/headline cx cy text scale)         ; big-glyph title (scale 4–5)
(ui/status-glyph col row state)        ; running ● / starting ◐ / error ✖ / stopped ○

;; lists
(ui/list cx cy cw ch rows selected leading-glyph-fn)
(ui/table cx cy cw ch cols rows frozen-cols header-glyphs)

;; data
(ui/sparkline cx cy cw data)           ; time-series via ▁▂▃▄▅▆▇█
(ui/fine-bar  cx cy cw value max)      ; sub-cell horizontal value bar

;; compositing
(ui/select-row cx cy cw)               ; invert a row span (selection channel)
(ui/modal cx cy cw ch title body-fn)   ; scrim + shadow + double-cbox + body
(ui/palette items prompt)              ; centered command palette
```

The full per-component contracts (and the design rationale behind each) are in
the [UI Design Language](/kn86-ui/ui-design-language/) §10.

## Building / testing

```sh
cmake -S . -B build && cmake --build build
(cd build && ctest --output-on-failure)
```

The component tests load `render/` + `ui/` into a System-tier KEC context, eval
a component form, and assert the in-memory render surface glyph-by-glyph (a
golden-buffer harness). No SDL — the tests run headless.
