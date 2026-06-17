---
title: UI Design Language
description: "The amber-on-black design language: surface, four-channel rule, type scale, the ui/ component kit, and the screen schema."
---

:::note
Extracted from [kn86-docs](https://kn86-deckline.com), the canonical engineering docs. ADR and cross-doc links resolve there.
:::


**Status:** Draft v0.1 — design doc / working spec, for Josh review + iteration. Not yet an ADR. Companion to [ADR-0036](../adr/ADR-0036-native-framebuffer-renderer.md) (the renderer decision this doc cashes out) and to the [Fe-Lisp Runtime Architecture draft](fe-lisp-runtime-architecture.md) (PR #75) §5 `ui/` row (the library this doc specifies).
**Date:** 2026-06-13
**Author:** PM / Architect (grounded against the on-glass session against the production `deckline` prototype)
**Extends:** [ADR-0036](../adr/ADR-0036-native-framebuffer-renderer.md) (native framebuffer renderer + EMBER phosphor), [ADR-0034](../adr/ADR-0034-aesthetic-mode-mechanism.md) (aesthetic-mode roster — pending the ADR-0036 amendment to `EMBER / WHITE / GREEN`), [ADR-0033](../adr/ADR-0033-reveal-noshapi-primitive.md) (reveal grammar), [`docs/influences/synthesis-batch8.md`](../influences/synthesis-batch8.md) §1–§2 (UI pattern catalog + single-color adaptation), [`docs/software/api-reference/grammars/character-set.md`](../character-set/) (KN-86 Code Page Layer 1 + the ADR-0036 §"Validation reference" custom-glyph extension)
**Ratification path:** this draft becomes an ADR once the §10 component-kit signatures and the §11 screen-spec schema are confirmed against the workbench's first emitted screens. Until then it is the working contract every screen the workbench emits is built against.

---

## 1. Context — the on-glass session

On **2026-06-13**, the PM and Josh ran a sequence of layout, compositing, and overlay tests directly against the production `deckline` prototype. The driver was `/home/kn86/fb_restest.py`, a ~200-line Python utility that paints candidate screens straight to `/dev/fb0` from the existing `kn86_font[256 × 8]` array. Each layout, each overlay, each phosphor candidate was *seen on the panel* before any of it was specified — and the visual vocabulary captured in this doc is the vocabulary that held up on glass.

The patterns documented below were validated in that order: cell-size hierarchy candidates (§5), then four-panel layout compositions (§7), then on-top-of-windows overlays (§8 / §9), then phosphor-color candidates (§3). Cross-references back to the test utility runs are noted at each section. The full validation log lives in [ADR-0036](../adr/ADR-0036-native-framebuffer-renderer.md) §"Validation reference"; this doc takes that ADR's "what we saw" as input and turns it into "what we build."

This doc is the design counterpart to ADR-0036's engineering counterpart: ADR-0036 specifies the surface (the 1024×600 framebuffer, the integer-scaled glyph table, the phosphor scheme switch); this doc specifies what you draw onto that surface and how the pieces compose.

---

## 2. Canonical surface (compact restatement)

Per [ADR-0036](../adr/ADR-0036-native-framebuffer-renderer.md) and the CLAUDE.md Canonical Hardware Specification as amended by it:

- **Surface:** 1024×600 RGB565 framebuffer. On device, `/dev/fb0` directly. On desktop, an SDL3 RGB565 surface SDL3 presents. No letterbox, no logical-vs-physical math.
- **Base glyph:** `kn86_font[256 × 8]` — the KN-86 Code Page (Press Start 2P + CP437 + hand-drawn), 8×8 bitmap, MSB-first, as specified in [`character-set.md`](../character-set/). No font format change from ADR-0027.
- **Type sizing:** integer scaling of the 8×8 glyph. A glyph at scale **s** occupies **8s × 8s** pixels. Per-element, freely composable on the same screen.
- **Density:** the 1× cell ceiling is 128 cols × 75 rows (1024/8 × 600/8). 80×25 is the 12×24 view (8×8 scaled 1×h/2×v, centered in a 12×24 cell). Both are equal-citizen views of one surface; neither is "the grid."
- **Chrome reservation (1× view):** Row 0 = runtime-owned status bar; Row 74 = runtime-owned action bar; Rows 1–73 = cart content. Carts never write Row 0 or Row 74.

This section restates the surface so the rest of this doc is self-contained; the authoritative specifications are CLAUDE.md and ADR-0036. If this section disagrees with either, those documents win — file a fix-up against this doc.

---

## 3. Phosphor color schemes

The aesthetic-mode axis is **foreground phosphor color** — a per-session choice that recolors every drawn pixel from the same monochrome surface. The retired ADR-0014 "glyph treatment" axis is gone; the ADR-0027 amber-only constraint is gone; the ADR-0036 phosphor roster is the canonical aesthetic-mode mechanism (per its companion amendment to ADR-0034, action item open as of this doc's date).

| Scheme | Hex | RGB565 | Role |
|---|---|---|---|
| **EMBER** | `#D84810` | `0xDA42` | **Canonical default.** Warmer than the retired AMBER; on-glass selected 2026-06-13. |
| **WHITE** | `#F0F0F0` | `0xF79E` | Alternate. P4-style phosphor. **Starting value pending on-glass tuning** (§12 OQ-1). |
| **GREEN** | `#33F033` | `0x3FE6` | Alternate. P1-style phosphor. **Starting value pending on-glass tuning** (§12 OQ-1). |

The background is fixed `0x0000` (true black). One foreground hue is active per session; the operator picks via the SYS-tab cycler (per the picker contract in [`bare-deck-terminal.md`](../software/runtime/bare-deck-terminal.md) §"TAB 4: SYS") and the choice persists to `/home/shared/nosh-config.toml`. Carts read the active scheme via `(get-aesthetic-mode) → :ember | :white | :green` per ADR-0036's amendment of ADR-0034.

**Hard constraints, no exceptions:**

1. **One foreground hue per session.** No multi-color palettes, no per-element overrides. A panel is the active hue or it is black-on-the-hue (inversion, §4); there is no third option.
2. **No antialiased proportional type.** Every glyph is an integer scaling of an 8×8 bitmap. Subpixel positioning, hinting, and anti-aliasing are out of scope at all scales — they break the phosphor-on-black aesthetic the surface is built around.
3. **No second hue, ever.** Hazard, warning, success, focus — all signaled in the four-channel rule (§4), never by introducing a second color.

The "selectable phosphor with monochrome discipline" stance is the load the entire UI design language carries; everything in §4–§10 is built on the premise that there is one foreground hue and black, and every other channel is built out of the same monochrome surface.

---

## 4. The single-color discipline (the four-channel rule)

Distilled from the Batch 8 synthesis ([`synthesis-batch8.md`](../influences/synthesis-batch8.md) §2.1) and validated on glass against the prototype on 2026-06-13. The corpus converged on this rule independently across [BrogueCE](../influences/inspiration/brogue-ce.md), [logradar](../influences/inspiration/logradar.md), and [durdraw](../influences/inspiration/durdraw.md); KN-86 adopts it as the binding constraint on every drawn surface.

**The four channels — in priority order:**

1. **Glyph = identity.** *What a thing is* is encoded by a distinct character per kind. A status icon for "running" is `●`, "starting" is `◐`, "error" is `✖`, "stopped" is `○`. A typed-column header is `$` for currency, `%` for percent, `@` for address. Never color, never position-alone. If two distinct kinds of thing share a glyph, the design is wrong; pick a new glyph.

2. **Inversion (black on phosphor) = selection / focus / hazard.** A cell renders in its inverted color pair to mean the operator is on it, the field has focus, or the value is dangerous. This is the only "active" channel. A list's selected row is inverted; a focused panel's title-cutout is inverted; a critical-threshold value is inverted. No other channel signals selection.

3. **Character density (`░▒▓█`, `▁▂▃▄▅▆▇█`) = magnitude.** *How much* of a thing exists is encoded by the density ramp. `░▒▓█` (CP437 0xA5–0xA2, 4 steps) covers area (heatmaps, scrim density). `▁▂▃▄▅▆▇█` (in the ADR-0036 custom-glyph extension at `0x96–0x9D` per §"Validation reference") covers bars and sparklines (8 sub-cell vertical steps). Neither carries category; both carry quantity.

4. **Border weight + leading-glyph column + blink = the orthogonal attention channels.** These three together carry per-region focus state, per-row state, and per-cell attention without consuming either the glyph slot (identity) or the inversion bit (selection):
   - **Border weight** — single-line (`─`/`│`) is a normal panel; double-line (`═`/`║`) is the focused panel or a modal. (§6.)
   - **Leading-glyph column** — column 0 of a list row carries a reserved state glyph: `●` unread, `*` pinned, `▸` selected-for-batch, ` ` (space) normal. The row's content starts at column 1.
   - **Blink / glyph-cycle on the 20 fps tick** — a cell cycles its glyph or inverts every other frame to flag liveness, anomaly, or "this needs your attention." Use sparingly; one blinking element per screen is the budget. More than that and the screen reads as broken.

Restated as a single rule, the way the corpus does: **color-as-category → glyph-as-category; color-as-magnitude → block-density-as-magnitude; selection/emphasis → inversion. Border weight, leading-glyph column, and blink fill in the rest.**

This is the rule every component in §10 obeys, and the rule the §11 screen schema's `components` field is built around.

---

## 5. Type hierarchy via integer scaling

The mechanism: integer scaling of the 8×8 KN-86 Code Page glyph, per element, freely composed on one screen. A glyph at scale **s** occupies **8s × 8s** pixels. Validated on-glass via `fb_restest.py 1` through `fb_restest.py 6` (six candidate uniform cell sizes) and `fb_restest.py lay 1`–`lay 4` (the four mixed-scale layout compositions).

The composable scales the on-glass session validated as readable on the Elecrow panel at normal viewing distance:

| Scale | Pixel cell | 1024×600 capacity | Role |
|---|---|---|---|
| **1×** | 8 × 8 | 128 × 75 | **Body.** Dense lists, tables, prose, mission ledgers, sparkline histories, log scrollback. The maximum-density configuration. |
| **2×** | 16 × 16 | 64 × 37 | **Chrome + subtitle.** Row 0 status bar, Row 74 action bar, panel titles, secondary headings, command-palette prompt rows. |
| **3×** | 24 × 24 | 42 × 25 | **Emphasis.** Inline value highlights, focused-row magnifications, "this matters" callouts. |
| **4×** | 32 × 32 | 32 × 18 | **Numbers + clock.** Big-glyph countdowns, credit balances, score indicators — anywhere a number needs to read across the room. |
| **5×** | 40 × 40 | 25 × 15 | **Headlines.** Mission titles, screen titles, "CONTRACT BOARD" header, big-glyph modal banners. |

Scales above 5× are admissible but degrade — the 8×8 pixel character at 6×+ reads as a blocky lo-fi sprite, which is sometimes the goal (a Headline at 8× for a defcon flash) and sometimes not. §12 OQ-2 carries the open decision on whether high-scale headlines stay integer-scaled (lo-fi by design) or get hand-drawn larger sprites.

### 5.1 Worked example — a mission board screen

The L1 layout from §7, laid out at four scales on one frame:

```
┌────────────────────────────── 1024 × 600 ──────────────────────────────┐
│ Row 0 (scale 2 chrome, 16px tall): [KZN-7A2B] [ ¤ 4,210 ] CIPHER → +14│
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│   CONTRACT BOARD                            ┌─────────────────────┐    │
│   (scale 5 title, 40px tall)                │  T-00:14:08         │    │
│                                             │  (scale 4 timer,    │    │
│   1× body ledger here ─────                 │   32px tall)        │    │
│   ─ 128-col cell-wide list of contracts     └─────────────────────┘    │
│   ─ each row at scale 1, body text          ┌─────────────────────┐    │
│   ─ leading-glyph state column              │  selected contract  │    │
│   ─ sparkline column (logradar)             │  detail panel       │    │
│                                             │  (scale 1 body)     │    │
│                                             │                     │    │
│                                             └─────────────────────┘    │
│                                                                        │
├────────────────────────────────────────────────────────────────────────┤
│ Row 74 (scale 2 chrome, 16px tall): EVAL accept · CDR next · BACK exit│
└────────────────────────────────────────────────────────────────────────┘
```

Four scales on one screen, all integer multiples of the same 8×8 glyph, all painting from the same `kn86_font` table — no separate font assets, no separate render paths, no impedance mismatch with the cell grid. Carts at the 1× cell tier author dense ledgers; the system tier (the chrome rows, the titles, the countdown) writes the mixed-scale headlines and the big-glyph timer.

Per ADR-0036's "What ADR-0027 we keep," carts stay at the 1× cell API (`cell-set` / `cell-print`); the mixed-scale rendering is system-tier work via `(draw-text x y scale fg text)` per [Fe-Lisp Runtime Architecture draft](fe-lisp-runtime-architecture.md) §4b. §12 OQ-3 carries the open decision on whether carts ever get scaled text.

---

## 6. Box drawing — windows and panels

Box-drawing is the panel primitive. Every framed region on screen — list panel, detail pane, modal, status frame — uses the same set of CP437 glyphs from the KN-86 Code Page (constants in `nosh_cart.h`, per [`character-set.md`](../character-set/)):

| Weight | Corners | Edges | Use |
|---|---|---|---|
| **Single-line** | `┌ ┐ └ ┘` (`0x82–0x85`) | `─ │` (`0x80 0x81`) | **Panels.** The normal weight for every framed region. |
| **Double-line** | `╔ ╗ ╚ ╝` | `═ ║` | **Modals + dialogs + the focused panel.** Heavier reads as "active" or "on top." |

Border weight is the §4 channel for *panel focus* — the focused panel renders its frame in double-line; siblings render in single. A modal rendered on top of a windowed scene gets a double-line frame regardless of underlying focus. Only one element on screen renders in double-line at any moment.

### 6.1 The title cutout (the bug we found on-glass)

A panel with a title in its top border must **clear the cells under the title before drawing the title text**. Otherwise the horizontal border line `─` runs through the title characters and the title reads as garbled or struck-through. The on-glass test on 2026-06-13 surfaced this on layout L2; the fix is the cutout pattern below.

```
WRONG:                              RIGHT:
┌─CONTRACT─BOARD─────────┐          ┌─ CONTRACT BOARD ───────┐
│                        │          │                        │
│  ...                   │          │  ...                   │
└────────────────────────┘          └────────────────────────┘

(border line stamps through         (title cells cleared, one-cell
 the title cells)                    pad on each side, border resumes)
```

The pattern:

1. Draw the full bordered box (top row gets the unbroken `─` line).
2. Compute the title's cell-width including a one-cell pad on each side: `w_cut = len(title) + 2`.
3. Compute the starting column for the cutout: `cx + 2` (offset 2 from the left corner, conventional placement).
4. Overwrite that span on the top row with: ` ` `title` ` ` — space, title text, space — drawn at scale 1.

The signature for the library function this becomes:

```
cbox(cx, cy, cw, ch, title, weight)
   cx, cy     — top-left in cells
   cw, ch     — width / height in cells
   title      — string or nil; if nil, no cutout
   weight     — 'single or 'double
```

`cbox` is one of the §10 primitives. Every panel on every screen is a `cbox` call.

---

## 7. The four canonical layouts (validated on-glass)

The on-glass session demonstrated four four-panel arrangements. Each composes the demonstrated panels — **transactions ledger** (long, scrollable), **captured files** (medium, with an encrypted-marker leading column), **countdown timer** (small, big-glyph numbers), and **mission log** (long, append-only) — into a screen-wide composition. Each layout suits a different design intent; the four are the canonical compositions cart and system surfaces draw from.

All cell coordinates below are at the 1× cell view (128 cols × 75 rows, Rows 1–73 cart-usable). Run references are `fb_restest.py lay 1` through `lay 4`.

### 7.1 L1 — ledger-left / ops-right (`fb_restest.py lay 1`)

```
Row 1  ┌─ TRANSACTIONS ──┐ ┌─ CAPTURED FILES ──────────────────┐
       │                 │ │                                   │
       │ scrollable      │ │  ● enc.dat       ▒ 2.4MB          │
       │ ledger, full    │ │    log.txt       ░ 18KB           │
       │ height          │ │    plans.kn      ▒ 412KB          │
       │                 │ └───────────────────────────────────┘
       │                 │ ┌─ T-00:14:08 ──────────────────────┐
       │                 │ │   (scale 4 countdown)             │
       │                 │ └───────────────────────────────────┘
       │                 │ ┌─ MISSION LOG ─────────────────────┐
       │                 │ │  > intrusion clean                │
       │                 │ │  > 2 files captured               │
       │                 │ │  > timer armed                    │
Row 73 └─────────────────┘ └───────────────────────────────────┘

       cols 0–41          cols 42–127
```

- **Left panel:** transactions ledger spans Rows 1–73, cols 0–41 (42 cols wide). The scrollable surface.
- **Right column** (cols 42–127, 86 cols wide) stacks three panels top-to-bottom: captured-files (~rows 1–22), countdown (~rows 23–37), mission-log (~rows 38–73).
- **Use when:** the ledger is the primary surface and the operator's eye lives there. Right-column panels are reference/status, not primary.

### 7.2 L2 — 2×2 quadrants (`fb_restest.py lay 2`)

```
Row 1  ┌─ TRANSACTIONS ──────────┐ ┌─ CAPTURED FILES ────────┐
       │                         │ │                         │
       │                         │ │                         │
       │                         │ │                         │
Row 36 └─────────────────────────┘ └─────────────────────────┘
Row 37 ┌─ COUNTDOWN ─────────────┐ ┌─ MISSION LOG ───────────┐
       │                         │ │                         │
       │                         │ │                         │
       │                         │ │                         │
Row 73 └─────────────────────────┘ └─────────────────────────┘

       cols 0–63                   cols 64–127
```

- Equal-weight 64×36 / 64×37 quadrants. None of the four dominates.
- **Use when:** no single panel is the primary surface. Reading mode for a multi-panel dashboard where each panel carries equal information density.

### 7.3 L3 — timer banner + 3 columns (`fb_restest.py lay 3`)

```
Row 1  ┌────────────── T-00:14:08 ────────────────────────────┐
       │   (scale 4 countdown, banner-wide)                   │
Row 14 └──────────────────────────────────────────────────────┘
Row 15 ┌─ TRANSACTIONS ──┐ ┌─ CAPTURED FILES ─┐ ┌─ MISSION ──┐
       │                 │ │                  │ │  LOG       │
       │                 │ │                  │ │            │
       │                 │ │                  │ │            │
       │                 │ │                  │ │            │
       │                 │ │                  │ │            │
Row 73 └─────────────────┘ └──────────────────┘ └────────────┘

       cols 0–41          cols 42–84             cols 85–127
```

- Top banner (Rows 1–14, full width) carries the countdown at scale 4 — the urgency anchor for the screen.
- Bottom: three equal columns (cols 0–41 / 42–84 / 85–127) carry the data panels.
- **Use when:** the timer is the urgency anchor. The operator's eye snaps to the top first, then the three columns.

### 7.4 L4 — command center (`fb_restest.py lay 4`)

```
Row 1  ┌─ TRANSACTIONS ─┐ ┌─ CAPTURED FILES ─┐ ┌─ T-00:14:08 ─┐
       │                │ │                  │ │ (scale 4)    │
       │                │ │                  │ │              │
       │                │ │                  │ │              │
Row 36 └────────────────┘ └──────────────────┘ └──────────────┘
Row 37 ┌─ MISSION LOG ──────────────────────────────────────────┐
       │ > intrusion clean                                      │
       │ > 2 files captured                                     │
       │ > timer armed                                          │
       │ ...                                                    │
Row 73 └────────────────────────────────────────────────────────┘
```

- Top half (Rows 1–36): three panels side-by-side carrying the operational state.
- Bottom half (Rows 37–73, full-width): the mission log as the running narrative spine — every event the run produces lands here in order.
- **Use when:** the mission log is the operational narrative and everything else is in service to it. The log is the screen's center of gravity.

### 7.5 Layout selection

The four layouts are not exhaustive — a screen can specify custom panel rectangles via the §11 schema's `layout` field. But the four cover the dominant compositions on-glass: **L1** when one panel dominates and the rest are reference; **L2** when the panels are equal-weight; **L3** when a single value (the timer) is the urgency anchor; **L4** when the narrative is the spine. A screen author picks one and then names its panels; departing from L1–L4 is admissible but should be a deliberate choice, documented in the screen's spec.

---

## 8. Compositing — drawing on top of windows

The on-glass session validated two depth cues that read correctly in a single-color phosphor surface. Both were demonstrated by `fb_restest.py pal` (command palette) and `fb_restest.py hint` (IntelliSense overlay). The two cues, plus the §4 inversion channel, compose into every overlay the system tier needs.

### 8.1 The two depth cues

1. **Dither scrim.** Knock out a 2px diagonal-dither pattern across a base region to read it as *dimmed / behind*. The implementation is a pixel-level XOR of a `0x55 0xAA 0x55 0xAA …` checkerboard mask against the base region's pixels — half the pixels turn off, half stay on. The result reads as a screened-back version of the original. Reads because the contrast between solid-foreground (the modal on top) and half-lit dither (the base behind it) is real and immediate. The base is still legible enough to confirm what's underneath; it's not legible enough to compete for the operator's attention.

2. **Drop shadow.** A solid-black rectangle offset 1 cell down and 1 cell right from a modal's frame. Eight pixels wide (one 1× cell column) on the right, eight pixels tall (one 1× cell row) on the bottom. Only reads as a shadow *because the base behind is scrim'd* — on a non-scrim'd base, the solid-black shadow disappears into the underlying black background and the modal appears to float over nothing. The scrim is what gives the shadow contrast; the shadow is what gives the modal depth. Neither cue works alone.

### 8.2 Inverted-row selection (the third compositing primitive)

The §4 inversion channel composes through to overlays: a list inside a modal selects its focused row by inverting it (phosphor background + black foreground). This is the third compositing primitive — not a depth cue, but the focus signal that overlays carry forward from the underlying §4 rule.

### 8.3 The three composed primitives

These three cues compose into three system-tier primitives that every overlay in the runtime is built from:

```
modal(cx, cy, cw, ch, title)
   — clears its interior to black (overwrites whatever was there)
   — draws a drop shadow (solid black, 1-cell offset, +cw on right, +ch on bottom)
   — draws a double-line bordered cbox (per §6) with title cutout
   — pre: caller has applied scrim() to the underlying region (or to the whole screen)

popup(anchor, items, signature?)
   — modal anchored to a position (anchor = cell coordinate, typically a cursor)
   — items rendered as a list inside the modal, selected row inverted
   — optional signature string docked one row below the modal as a tooltip
   — used for IntelliSense completion (§9.2)

palette(items, prompt)
   — centered modal (auto-positioned at screen center)
   — prompt row at top in scale 2 (the command palette's "type a command" prompt)
   — separator row (single-line ─, full width of modal interior)
   — ranked items list, selected row inverted
   — keybinding footer row at bottom in scale 1 (ENTER / ESC / TAB hint strip)
   — used for the command palette (§9.1)
```

`modal` is the workhorse. `popup` and `palette` are specializations. Every overlay in the runtime — IntelliSense, command palette, yes/no dialogs, form-input panels — bottoms out in `modal`.

---

## 9. Overlay patterns (validated on-glass)

Three overlay patterns covered the on-glass tests, and three patterns cover every system-tier overlay we expect to need.

### 9.1 Command palette (`fb_restest.py pal`)

**When:** the operator presses TERM in a context that exposes commands (nEmacs, REPL, SYS shell, any system surface with a command vocabulary).

**Surface:** centered modal, ~64 cells wide, ~24 cells tall (1× cell view). The underlying screen is scrim'd. The modal:

- **Prompt row** (top, scale 2): `> ` followed by the operator's typed query. Cursor blinks at the end.
- **Separator** (one row down): single-line `─` spanning the modal's interior width.
- **Ranked items list** (the bulk of the modal): one command per row, scale 1, leading-glyph column carries a `▸` for the selected row, the row itself is inverted. Items are filtered live from the typed query; the ranking is mcfly-style (recent + frequent + match-quality), per [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §1.4.
- **Keybinding footer** (bottom row, scale 1): `ENTER prep run   ESC cancel   TAB next`.

ENTER prep-runs the selected command (the command's args are filled and the runtime presents a confirmation); ESC cancels and dismisses the palette; TAB cycles to the next item in the ranked list.

### 9.2 IntelliSense completion (`fb_restest.py hint`)

**When:** the operator types in nEmacs (or any system-tier editor surface) and the cursor sits after a partial symbol that has match candidates in the Fe symbol table.

**Surface:** a `popup` anchored under the cursor. The popup:

- **Matches list:** one symbol per row, scale 1. The leading-glyph column carries a `λ` prefix for callable symbols (functions, lambdas), space for everything else (variables, special forms, macros). The selected match is inverted.
- **Signature tooltip** (one row below the modal, no border): scale 1 text rendering the selected symbol's signature — `(define (snake-step s) …)` or `(define grid 128)` or `(define-syntax when …)`. Dismissed when the popup is dismissed.

Filter matches live from what the operator has typed against the Fe symbol table. This implements the s-expression-browser pattern from [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §4.3: Fe is the language, the live symbol table is the browse target, and completion is the operator's read of that table at the cursor.

ENTER inserts the selected symbol; ESC dismisses the popup; TAB cycles. The cart underneath never sees the operator's keystrokes during the popup — the system tier owns the keystream until the popup is dismissed.

### 9.3 Dialog

**When:** the runtime needs a yes / no / cancel from the operator, or a form-input from the operator. Confirmation modals, "are you sure?" prompts, the SYS-tab's deck-wipe affordance, mission-config forms.

**Surface:** centered `modal`. The dialog body is laid out at scale 1 within the modal's interior — prompt text, an optional form region (`form`, per §10), and a row of buttons at the bottom (each rendered as `[ YES ] [ NO ] [ CANCEL ]`, the focused button inverted). TAB cycles the focused button; ENTER invokes the focused button; ESC invokes CANCEL.

Same compositing primitives as `modal` and `palette` — the dialog adds no new mechanism, just a button-row idiom on top of the shared overlay surface.

---

## 10. The `ui/` component kit (the build payload)

This is the cash-out of the [Fe-Lisp Runtime Architecture draft](fe-lisp-runtime-architecture.md) §5 `ui/` row. Every component below is one of the primitives a Fe screen calls; every component obeys the §4 four-channel rule; every component composes through the §11 schema.

Components are listed by domain. Each row: signature, one-line purpose, the Batch 8 seed that motivated it.

### 10.1 Chrome rows

| Component | Signature | Purpose | Seed |
|---|---|---|---|
| **`status-bar`** | `(status-bar handle credits header)` | Row 0 chrome. Scale 2, inverted. Handle + credits + section header, `▸`-separated. Runtime-owned. | [logradar](../influences/inspiration/logradar.md) (sources ▸ patterns ▸ events ▸ evt/m) |
| **`action-bar`** | `(action-bar bindings)` | Row 74 chrome. Scale 2, inverted. `bindings` = list of (key, label) pairs. Runtime-owned. | [micro](../influences/inspiration/micro.md) (context-sensitive bottom strip), [mc](../influences/inspiration/mc.md) (digit+label+inverted-active) |

### 10.2 Frames

| Component | Signature | Purpose | Seed |
|---|---|---|---|
| **`panel`** / **`box`** | `(panel cx cy cw ch title)` → calls `cbox` single-weight | Single-line bordered region with optional title cutout. The default frame for every layout panel. | [desktop-tui](../influences/inspiration/desktop-tui.md), [tvterm](../influences/inspiration/tvterm.md) |
| **`cbox`** | `(cbox cx cy cw ch title weight)` | The raw box-drawing primitive (§6). `weight` ∈ `single`/`double`. Title cutout if `title` non-nil. | §6 |
| **`modal`** | `(modal cx cy cw ch title body-fn)` | Compositing primitive (§8.3). Drop shadow + double-line cbox + black interior fill. `body-fn` is called to draw the modal's contents. | [microsoft-edit](../influences/inspiration/microsoft-edit.md) |
| **`popup`** | `(popup anchor items signature?)` | Modal anchored to a cursor position. | §9.2 |
| **`palette`** | `(palette items prompt)` | Centered command palette modal. | [mcfly](../influences/inspiration/mcfly.md) |

### 10.3 Lists, trees, tables

| Component | Signature | Purpose | Seed |
|---|---|---|---|
| **`list`** | `(list cx cy cw ch rows selected leading-glyph-fn)` | Vertical scrolling list. Selected row inverted. `leading-glyph-fn` returns the §4 leading-glyph column per row. | [clipse](../influences/inspiration/clipse.md), [mynav](../influences/inspiration/mynav.md), [newsboat](../influences/inspiration/newsboat.md) |
| **`tree`** | `(tree cx cy cw ch root expanded? select)` | Collapsible tree with `▸` (collapsed) / `▾` (expanded) glyphs, indent + box-drawing for nesting. **The s-expression browser** — same widget for nEmacs and reader-mode content. | [hackernews-TUI](../influences/inspiration/hackernews-tui.md), [bookokrat](../influences/inspiration/bookokrat.md), [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §4.3 |
| **`table`** | `(table cx cy cw ch cols rows frozen-cols header-glyphs)` | Pinned header row (inverted) + scrolling body + optional frozen leftmost columns. Typed-column header glyphs (`$ % @ #`). | [csvlens](../influences/inspiration/csvlens.md), [sc-im](../influences/inspiration/sc-im.md), [visidata](../influences/inspiration/visidata.md) |

### 10.4 Data-display primitives

| Component | Signature | Purpose | Seed |
|---|---|---|---|
| **`sparkline`** | `(sparkline cx cy cw data)` | Time-series via vbars `0x96–0x9D` (the ADR-0036 §"Validation reference" custom-glyph extension). Logradar recipe: 24 buckets / 2-min window / soft-cap 3× mean / newest-bolded. | [logradar](../influences/inspiration/logradar.md), [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §2.3 |
| **`fine-bar`** | `(fine-bar cx cy cw value max)` | Horizontal progress / value bar with sub-cell precision via hbars `0xA8–0xAE` + `0xA2` full-block. 8× horizontal sub-cell resolution. | ADR-0036 §"Validation reference" custom-glyph extension |
| **`status-glyph`** | `(status-glyph col row state)` | Renders a state icon. `state` ∈ `running ● / starting ◐ / error ✖ / stopped ○`. Animates `starting` by frame-cycling. | [logradar](../influences/inspiration/logradar.md), [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §1.5 |

### 10.5 Navigation + input

| Component | Signature | Purpose | Seed |
|---|---|---|---|
| **`tab-bar`** | `(tab-bar cx cy tabs selected)` | Numbered tab strip (1–5). Selected tab inverted. Matches the Bare Deck STATUS/CIPHER/LAMBDA/LINK/SYS pattern. | [csvlens](../influences/inspiration/csvlens.md), [chronos](../influences/inspiration/chronos.md), bare-deck digit tabs |
| **`form`** | `(form cx cy cw fields focused-field)` | Multi-field input panel. Each field has a label + an inline editor; focused field inverted; TAB cycles fields. | [chronos](../influences/inspiration/chronos.md) (Huh-library form panel), [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §1.4 |
| **`dial`** | `(dial cx cy cw ch value min max)` | Continuous-control widget — a circular indicator with value readout. Drawn via half-block sub-pixels. | Toneline cluster, [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §1.4 |
| **`slider`** | `(slider cx cy cw value min max)` | Continuous-control widget — a horizontal bar with cursor position. | Toneline cluster |
| **`cycler`** | `(cycler cx cy label current options)` | Single-row cycle through a small enum (e.g., `MODE: EMBER › WHITE › GREEN`). Inverted on focus. The SYS-tab aesthetic-picker primitive. | [Fe-Lisp Runtime Architecture draft](fe-lisp-runtime-architecture.md) §6 worked example (b) |

### 10.6 Headlines + emphasis

| Component | Signature | Purpose | Seed |
|---|---|---|---|
| **`headline`** | `(headline cx cy text scale)` | Big-glyph headline at scale 4–5. Black-on-phosphor (or phosphor-on-black inverted-card variant via `weight 'card`). The "this is a screen / mission / event title" primitive. | §5 mission-board worked example |

### 10.7 Compositing + transition primitives

| Component | Signature | Purpose | Seed |
|---|---|---|---|
| **`scrim`** | `(scrim cx cy cw ch)` | Apply the dither scrim (§8.1). Reads as "dimmed / behind." | §8.1 |
| **`shadow`** | `(shadow cx cy cw ch)` | Draw a 1-cell drop shadow at the given rectangle. | §8.1 |
| **`select-row`** | `(select-row cx cy cw)` | Invert a single row span. The list-selection primitive. | §4 |
| **`reveal`** | `(reveal style)` / `(unreveal style)` | The canonical transition primitive per [ADR-0033](../adr/ADR-0033-reveal-noshapi-primitive.md). Modal entry/exit, cart load, ambient-mode entry/exit all dispatch through it. | [ADR-0033](../adr/ADR-0033-reveal-noshapi-primitive.md), [`reveal-styles.md`](../software/api-reference/grammars/reveal-styles.md) |

Components in §10.3, §10.4, §10.5, §10.6 are general-purpose and Fe-resident — implemented in `ui/` as Fe functions over the system tier's privileged drawing primitives. Components in §10.1, §10.2 (frames + chrome rows), and §10.7 (compositing) bottom out in the §10.2 `cbox` + the §8.3 `modal`/`popup`/`palette` + the §8.1 `scrim`/`shadow`/`select-row` primitives, which are themselves Fe wrappers over the system-tier `(draw-text)` / `(draw-box)` / `(draw-glyph)` / `(fill-rect)` / `(blit-bitmap)` per [Fe-Lisp Runtime Architecture draft](fe-lisp-runtime-architecture.md) §4b.

The cart tier (`cell-set` / `cell-print` / `half-block-set`) is unchanged from ADR-0027 and unaffected by the `ui/` kit; cart-authored UI uses the cart cell API directly, which is sufficient for the constrained Rows-1–73 surface. The `ui/` kit is system-tier work that paints the privileged surface.

---

## 11. Screen-spec schema (feeds the workbench)

Every system screen is a record. The schema below is the contract Josh's workbench (in a separate Claude Code thread) is built to edit and emit. A workbench-emitted screen compiles directly against the §10 `ui/` library — every component named in the schema's `components` field is one of the §10 primitives, every layout reference is L1–L4 (or a custom rectangle list), every handler / transition is Fe code.

### 11.1 The record

```
SCREEN  <name>
  surface       system | cart
  layout        <L1|L2|L3|L4|custom>   ; per §7
  phosphor      inherits | locked      ; locked pins the aesthetic mode at the screen
  overlays      [ <overlay-spec> ... ]
  components    [ <component-spec> ... ]
  state
    reads       [ <field-of-deck-state-or-cart-state> ... ]
    writes      [ <field> ... ]        ; rare; system tier only
  handlers
    on-key      { <key> → <action> ... }
    on-tick     { <body> }
  transitions   [ <event> → <screen-name> ... ]
```

**Fields:**

- **`surface`** — `system` for system-tier screens (mission board, nEmacs, REPL, deck utils, SYS tabs); `cart` for cart-authored screens. Determines which binding set the screen's Fe context is created with — `system` gets `term/*` and `ui/*`; `cart` gets only the cart cell API. This is the ADR-0027 / [Fe-Lisp Runtime Architecture draft](fe-lisp-runtime-architecture.md) §4 two-tier model exposed at the screen level.

- **`layout`** — one of `L1`, `L2`, `L3`, `L4` (per §7), or `custom`. If `custom`, the schema names panel rectangles inline:
  ```
  layout custom
    panels
      ledger    (0 1 42 73)
      timer     (44 2 40 12)
      log       (44 16 84 58)
  ```
  The layout reference fixes the panels' coordinates; the schema then binds named panels to components.

- **`phosphor`** — `inherits` (the screen uses the operator's persisted aesthetic-mode choice) or `locked` (a screen pins a specific scheme, e.g., the SYS-tab aesthetic-picker locks to `EMBER` regardless of current setting so the operator can see the default). Default is `inherits`.

- **`overlays`** — list of overlay specifications, anchored to coordinates or to anchors (the cursor, a panel, a specific row). Each overlay spec names one of the §9 patterns (`palette`, `popup`, `dialog`) and specifies when it's mounted (which key trigger, which event). Overlays are not always-on; they mount and unmount under the handlers' control.

- **`components`** — the bulk of the spec. Each row binds a §10 component name to data sources:
  ```
  components
    status-bar    handle credits "MISSIONS"
    list          panel=ledger    rows=(deck/contracts)    selected=(state/cursor)
    headline      panel=timer     text=(mission/countdown) scale=4
    sparkline     panel=stats     data=(deck/credits-history)
    action-bar    EVAL=accept  CDR=next  BACK=exit
  ```
  Each binding is a Fe call: `(list 0 1 42 73 (deck/contracts) (state/cursor) leading-glyph-fn)`. The schema names the component and the bindings; the emit pass writes the Fe call.

- **`state.reads`** — declares which fields of Universal Deck State or cart-local state the screen reads. The runtime can use this for cache invalidation (re-render only when a read field changes) and for the workbench's live-preview path (the workbench knows what to vary in the preview).

- **`state.writes`** — declares which fields the screen writes. System tier only; carts cannot declare writes to Universal Deck State (carts mutate their own state through the cart cell API, but they never write the integrity-tracked deck fields).

- **`handlers.on-key`** — a key-to-action map. Each action is either a Fe lambda or a named handler defined elsewhere in the same module. Keys are the canonical Sweep symbols (`EVAL`, `CDR`, `CAR`, `BACK`, `TERM`, digits, etc., per ADR-0031 §3.1). The runtime dispatches keys into the screen's `on-key` after the chrome-row owners (the runtime itself) have had first refusal.

- **`handlers.on-tick`** — a body called once per frame (under the 20 fps animation cap). For screens with animated content (sparkline updates, countdown ticks, blink-cycle effects). Absent → screen is event-driven and idles when no input.

- **`transitions`** — declares which events move to which other screens. The screen-to-screen graph is declared here so the workbench can render the navigation graph and the runtime can validate it at startup.

### 11.2 A worked example — the mission board

```
SCREEN  mission-board
  surface      system
  layout       L1
  phosphor     inherits
  overlays     [ (palette on-key TERM) ]
  components
    status-bar   handle=(deck/handle) credits=(deck/credits) header="MISSIONS"
    panel        name=ledger    title="CONTRACT BOARD"
    list         panel=ledger
                 rows=(board/contracts)
                 selected=(state/cursor)
                 leading-glyph-fn=board/row-glyph
    headline     panel=timer    text=(mission/countdown) scale=4
    panel        name=detail    title="CONTRACT"
    panel        name=log       title="MISSION LOG"
    list         panel=log
                 rows=(board/log)
                 selected=nil
                 leading-glyph-fn=(const " > ")
    action-bar   EVAL=accept   CDR=next   BACK=exit
  state
    reads        deck-state(handle credits reputation contracts cursor)
    writes       deck-state(cursor)
  handlers
    on-key       EVAL → (board/accept-selected st)
                       (phase/advance st)
                 CDR  → (board/move-cursor st +1)
                 CAR  → (board/move-cursor st -1)
                 BACK → (deck/goto st 'bare-deck)
                 TERM → (overlay/mount 'palette)
    on-tick      (board/tick-countdown st)
  transitions   accept → phase-chain
                BACK   → bare-deck
```

### 11.3 How §11 composes with §10

The schema is the structural spec; §10 is the implementation. Every name in `components` is a §10 primitive; every overlay is a §9 pattern bound to a trigger; every layout is L1–L4 (§7) or a custom rectangle list; every key is a Sweep symbol. The workbench's emit pass walks the schema and writes Fe — `status-bar` becomes `(ui/status-bar (deck/handle st) (deck/credits st) "MISSIONS")`, `list` becomes `(ui/list cx cy cw ch (board/contracts st) (state/cursor st) board/row-glyph)`, `action-bar` becomes `(ui/action-bar '(("EVAL" . accept) ("CDR" . next) ("BACK" . exit)))`, and so on. The schema gives the workbench a structural surface to edit and a deterministic emit target; the §10 library gives the runtime the components those calls bind to. The schema and the library are the same vocabulary expressed twice — once in a workbench-editable record, once as a Fe function call.

---

## 12. Open questions

1. **WHITE / GREEN exact hexes (§3).** EMBER `#D84810` is locked (on-glass selected). WHITE `#F0F0F0` and GREEN `#33F033` are starting values pending on-glass tuning — every monitor and panel slightly shifts the actual phosphor read; the production prototype is the only reference that matters and a follow-up tuning session will fix the alternates. Until then the starting values ship.

2. **Big-glyph font at scale 4+ (§5).** At scales above 3×, the 8×8 source character reads as a coarse lo-fi sprite. Three options:
   - **(a)** Ship the integer-scaled 8×8 as the canonical big-glyph (lo-fi by design — the device aesthetic carries it).
   - **(b)** Author hand-drawn 24×24 sprites for a small set of headline glyphs (digits 0–9, capitals, `λ` and the box-drawing weights).
   - **(c)** Provide both and let carts/system surfaces pick.
   - **Recommendation: (a) for v0.1.** The lo-fi reading *is* the aesthetic; if it becomes a real problem on the SYS-tab font picker, (b) lands as a follow-up sprite pack. (c) is over-engineering for v0.1. Defer.

3. **Cart access to scales (§5).** ADR-0036 §"What ADR-0027 we keep" preserves the constrained cart cell API at scale 1×. Open: do carts ever get a `(draw-text-scaled col row scale fg text)`, or do they stay at 1× and use big-glyph via sprite blits (system surface) only?
   - **Recommendation: carts stay at 1× for v0.1.** The constrained cart surface is a sandbox boundary; scaled text is a system-tier capability. Carts that want big-glyph emphasis use the half-block canvas at 128×150 (already in the cart surface per ADR-0036 §Decision-6) or compose §10's leading-glyph + inversion + density channels to draw attention within the 1× grid. The system tier draws the mixed-scale chrome around them. Revisit in v0.x if a launch-title spec hits a wall.

Once these three are resolved, this draft becomes an ADR and the §10 component list locks as the build backlog.

---

## 13. References

**Anchoring specs:** [ADR-0036](../adr/ADR-0036-native-framebuffer-renderer.md) (native framebuffer renderer + EMBER phosphor — the renderer this doc draws onto), [ADR-0034](../adr/ADR-0034-aesthetic-mode-mechanism.md) (aesthetic-mode mechanism — pending ADR-0036 amendment to `EMBER / WHITE / GREEN`), [ADR-0033](../adr/ADR-0033-reveal-noshapi-primitive.md) + [`reveal-styles.md`](../software/api-reference/grammars/reveal-styles.md) (reveal grammar), [`character-set.md`](../character-set/) (KN-86 Code Page Layer 1; the ADR-0036 §"Validation reference" custom-glyph extension adds vbars `0x96–0x9D` and hbars `0xA8–0xAE`), [`fe-lisp-runtime-architecture.md`](fe-lisp-runtime-architecture.md) (the §5 `ui/` row this doc cashes out).

**Pattern sources:** [`docs/influences/synthesis-batch8.md`](../influences/synthesis-batch8.md) §1 (UI pattern catalog), §2 (single-color adaptation), §4 (LISP integration patterns). The Batch 8 entries this doc cites are the entries [`synthesis-batch8.md`](../influences/synthesis-batch8.md) §6 enumerates; this doc names exemplars in-line and leaves the full corpus index to the synthesis.

**Hardware ground truth:** [CLAUDE.md](../../CLAUDE.md) Canonical Hardware Specification (the surface, the chrome rows, the phosphor roster — as amended by ADR-0036).

**Validation:** on-glass session 2026-06-13 against `ssh://deckline`. Test utility at `/home/kn86/fb_restest.py`. Run references throughout §5, §7, §8, §9 (`fb_restest.py 1`–`6`, `lay 1`–`4`, `pal`, `hint`, `colors`).
