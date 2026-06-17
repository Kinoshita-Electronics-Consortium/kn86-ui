---
title: Screen Design Rules
description: "Row authority, margins, density modes, and whitespace rules for KN-86 screens."
---

:::note
Extracted from [kn86-docs](https://kn86-deckline.com), the canonical engineering docs. ADR and cross-doc links resolve there.
:::


*Working scratch — decisions made during screen design sessions. Living document.*

> **CIPHER-LINE revision note (2026-04-24):** The "Cipher Ticker" on Row 21 of the main grid (§ Cipher Ticker) is **superseded**. Cipher utterances no longer appear on the main 80×25 grid; they render on the **CIPHER-LINE OLED** above the keyboard. The row-21 ticker slot is freed for other main-grid uses. The aesthetic intent of an ambient, always-alive voice is preserved by CIPHER-LINE's Ambient pattern (see `docs/software/cartridges/authoring/ui-patterns.md` §3A.4 Pattern 3A-A). The "Legend reputation goes silent" behavior is preserved by the mode selector's beat + cart-history bias (operators with deep cartridge history get `:silent`-heavier distributions). Canonical engine spec: `docs/software/runtime/cipher-voice.md`.

---

## 1. The Grid

> **Grid migration notes:**
> - **[ADR-0027](../../../adr/ADR-0027-termbox2-display-layer.md), ratified 2026-06-07:** retired the 80-column / Row-24 layout in favor of a 128×75 cell grid. The §1 figures below are updated to the new layout.
> - **[ADR-0036](../../../adr/ADR-0036-native-framebuffer-renderer.md), 2026-06-13:** native framebuffer renderer supersedes the termbox2 display layer. **128×75 is the 1× cell *ceiling*, not THE canonical grid** — the canonical surface is the 1024×600 RGB565 panel, and cell density is per-region via integer-scaled 8×8 glyphs (1× = 128×75 cells, 2× = 64×37, 3× = 42×25, etc.). The cell-API authoring shape (`(cell-set …)` / `(cell-print …)` / chrome reservation / two-tier model / half-block 128×150 canvas) is **preserved verbatim** from ADR-0027; only the C backend changed.
> - Detailed wireframes elsewhere in this doc and in module specs that still show 80-column / Row-24 art are part of the per-screen redesign tracked in the cartridge re-flow task — they have not all been re-drawn yet.

- **1× cell ceiling:** 128 columns × 75 rows (Rows 0–74) on the 1024×600 panel
- **Row 0:** Status bar — nOSh-runtime-owned, never scrolls, always visible *after boot completes*
- **Row 74:** Action bar — nOSh-runtime-owned, never scrolls, always visible *after boot completes*
- **Content area (1× cell):** Rows 1–73 = 73 rows × 128 cols = 9,344 cells
- **Mixed cell sizes:** Carts may compose 2×, 3×, or larger-scale regions on the same screen via the native renderer's integer-scale path (e.g., 32-px headline over 8-px body — see [ADR-0036](../../../adr/ADR-0036-native-framebuffer-renderer.md) and the [UI design language](../ui-design-language/) doc).
- **During boot:** Rows 0–74 are ALL available. Status bar and action bar don't exist yet — firmware hasn't loaded them. The full panel is a raw framebuffer.

---

## 2. Margins: Mixed Rule

- **Edge-to-edge** for structural elements: horizontal dividers (`────`), box borders (`┌┘`), full-width lines.
- **1–2 char indent** for text content: prose, field labels, body text.
- **Selection markers** (`[>]`, `[ ]`) naturally start at col 0 and push content to col 4 — the marker IS the margin.
- Structure touches the edges. Prose floats one notch in.

---

## 3. Visual Hierarchy (Monochrome Tools)

Ranked by visual weight, heaviest to lightest:

```
HEAVIEST  ╔══ Double-line box ══╗     Modals, critical alerts ONLY
          ┌── Single-line box ──┐     Focused cells, panel borders
          ════════════════════════     Major section dividers (rare)
          ────────────────────────     Minor section dividers
          ALL CAPS HEADER             Section titles
          Title Case Label            Field names (use sparingly)
          body text sentence case     Narrative, descriptions
LIGHTEST  (metadata in brackets)      Footnotes, scroll indicators
```

**Rule: Never skip more than one level.** A double-box shouldn't contain body text with no intermediate structure.

---

## 4. Section Headers: Bare by Default

```
MISSION STATUS                          Style A: bare header (DEFAULT)
Time Elapsed: ██████░░░ 4:26           Costs 1 row. Use for most sections.
```

```
MISSION STATUS                          Style B: underlined header
────────────────────────────────────    Costs 2 rows. Screen title (Row 1)
Time Elapsed: ██████░░░ 4:26           or major divisions ONLY. Once per screen max.
```

```
┌─ MISSION STATUS ────────────────┐    Style C: boxed header
│ Time Elapsed: ██████░░░ 4:26    │    Costs 2+ rows. Modals and alerts ONLY.
└─────────────────────────────────┘    Never for regular sections.
```

---

## 5. Whitespace Rules

- 1 blank line before a section header (except Row 1 — no leading space needed)
- 0 blank lines between a header and its content
- 0 blank lines between consecutive list/table rows
- 1 blank line after a section before the next section starts
- **Never 2 consecutive blank lines anywhere**
- In narrative text (Cipher voice): 1 blank line = paragraph break

---

## 6. Column Anchors

Not rigid — recurring positions that screens snap to when it makes sense:

```
Col  0    Selection markers, row labels, box borders
Col  2    Content start (1-space indent for prose)
Col  4    Content start after [>]_ marker + space
Col 39    Split pane divider position (│)
Col 40    Right pane content start
Col 55    Right-zone start (secondary data, wide layouts)
Col 60    Right-zone start (secondary data, standard layouts)
Col 79    Right edge (right-aligned terminal position)
```

**Key rule:** Numbers and currency right-align to a consistent column within their section. If a column of numbers ends at col 72, ALL numbers in that column end at col 72.

---

## 7. Information Density Modes

Choose ONE per screen. Don't mix modes on the same screen.

| Mode | Blank Lines | Usable Data Rows | Use Cases |
|------|-------------|-------------------|-----------|
| **Dense** | None between items | ~21 rows | Mission board, transaction lists, node grids |
| **Structured** | Between sections | ~15–17 rows | Inspectors, dashboards, deck state overview |
| **Narrative** | Paragraph spacing | ~12–14 rows | Cipher voice, mission briefings, first-boot story |

---

## 8. Truncation Rules

- **List item text:** Truncate with `..` at column limit. Never wrap.
- **Narrative text:** Word-wrap at available width. Never truncate mid-thought.
- **Numbers:** NEVER truncate. If a number doesn't fit, the layout is broken — fix the layout.
- **Operator handle:** 12 chars max, space-padded.
- **Module names:** Full name in content areas; 3–4 char abbreviation in status bar.

---

## 9. Time Format: Hex Epoch Clock

The KN-86 runs on Unix epoch time internally. 1988 timestamps range from `0x21D84E00` (Jan 1) to `0x23C345FF` (Dec 31).

### Status Bar TIME Field — Three States

| State | Format | Example | Chars |
|-------|--------|---------|-------|
| Idle (no mission) | `T:` + 6 uppercase hex digits (low 24 bits) | `T:D84E00` | 8 |
| Mission elapsed | `T+` + MM:SS | `T+04:26` | 7 |
| Mission countdown | MM:SS + `▼` (warning when <1:00) | `05:34▼` | 6 |

### Hex Clock Behavior

- Ticks once per second (not per frame). Rightmost digit changes: `0→1→...→9→A→B→C→D→E→F→0`
- 6 hex digits = 16,777,216 values ≈ 194 days before rollover. The device cycles ~2×/year.
- `T:` prefix means "time register" — device convention, not programming convention.
- Full 8-digit epoch stored internally; display is intentionally truncated.
- On the Bare Deck Terminal (home screen): may show full 8-digit epoch prominently in the content area.

---

## 10. Motion Design: Scrolling Text

### Vertical Scroll Patterns

**Pattern A: Terminal Scroll (append-and-push)**
New lines appear at bottom, everything above scrolls up. The device "thinks out loud." Operator doesn't control the pace — the device does.
*Use for:* Boot POST, cartridge loading, system image updates, process output.

**Pattern B: Feed Scroll (log stream)**
Like Pattern A but content is incoming external data, not device output. Each line may have a source tag or timestamp prefix.
*Use for:* Intercepted transmissions, network traffic monitoring, transaction stream.

**Pattern C: Cycling/Flicker (in-place replacement)**
A field or region updates rapidly — overwriting in place, not scrolling. Values flicker through candidates. Locked values stay stable; unlocked ones blur.
*Use for:* Password cracking, hash collision attempts, LFSR prediction, key derivation.

### Horizontal Scroll Patterns

**Ticker (right-to-left crawl)**
Text moves R→L across a single row (or 2–3 rows). A window onto an infinite horizontal stream. Content enters from the right edge, exits the left edge.
*Use for:* Network traffic crawl, intercepted transmissions, market feed, ambient data.

**Signal Sweep (left-to-right trace)**
A cursor/highlight travels L→R across a row or block of rows, leaving a data trail. Wraps to left and overwrites on next pass.
*Use for:* Sonar ping (Depthcharge), RF scan (Drift), signal intercept (Shellfire).

**Marquee Overflow**
When a label/value exceeds its allocated column width, it slowly scrolls horizontally within its bounds. Functional, not decorative. Should be rare — good layout avoids it.

**Horizontal Cascade (typewriter reveal)**
Characters appear one at a time, left to right, within a row. Like a teletype printing. Row fills, then the next row starts. Vertical scroll at the row level, horizontal reveal at the character level.
*Use for:* Cipher voice, POST check lines, dramatic reveals.

### Combined Motion

- **Vertical scroll + static frame:** Process output flows up in a scroll region, surrounded by static labels and progress indicators.
- **Horizontal ticker + static frame:** A ticker row embedded in an otherwise static dashboard.
- **Vertical scroll + horizontal cascade:** Each new line types out L→R before the next line begins. POST checks type out, then result snaps in.
- **Signal sweep + vertical history:** Sweep crosses horizontally; on completion, results drop into a vertical log below.

### Scroll Regions

- **Implicit boundaries.** No box borders around scroll areas. The static content above and below defines the boundary. The scroll region is negative space.
- **During boot:** Full-screen scroll. Rows 0–24 are all scroll region.
- **During gameplay:** Scroll region is a subset of Rows 1–23, defined by surrounding static content.

### Scroll History: Gone Forever

When a line scrolls off the top (vertical) or exits the left edge (horizontal), it's gone. No scrollback buffer. You had to be watching.

**THE PERSISTENCE RULE:** If information is required for the operator to progress or make a decision, it MUST exist in a persistent, retrievable location — a static field, a cell accessible via INFO, or a record in the deck state. Scrolling text is for process feedback, atmosphere, and ambient intelligence. Critical data must also land somewhere non-ephemeral. The operator should never be stuck because they blinked.

### Speed Classes

| Class | Vertical Rate | Horizontal Rate | Audio | Use Cases |
|-------|---------------|-----------------|-------|-----------|
| Deliberate | 1 line / 500–800ms | 2–4 chars/sec | Individual line tones | Boot POST, cartridge load, Cipher typewriter |
| Brisk | 1 line / 100–200ms | 10–20 chars/sec | Rapid soft clicking | nOSh runtime update, file enum, scan results |
| Torrent | 1 line / 20–50ms | 40–80 chars/sec | Continuous noise wash | Data exfil, brute-force log, traffic dump |
| Flicker | In-place, 5–15fps | In-place, 5–15fps | High-freq warble → lock tone | Password cycling, hash attempts, LFSR predict |
| Sweep | N/A | Full-width in 1–3sec | Rising tone tracks cursor position | Sonar ping, RF scan, signal trace |

### Result Snap

When a scrolling or cascading process reaches a conclusion for a given line or field, the result appears **instantaneously** — no animation, no typing. It *snaps* into place.

```
Checking memory banks.......... OK       ← dots type out, OK snaps in
Probing cartridge slot......... EMPTY    ← same pattern
Verifying firmware CRC......... OK
Loading nOSh v1.2.4........... ████████  ← bar fills, then snaps to full
```

The snap is always accompanied by a distinct audio tone:
- `OK` / success: Crisp confirmation tone
- `EMPTY` / neutral: Mid-level tone
- `FAIL` / error: Error buzz
- Progress bar completion: Rising tone resolving to confirmation

The dots are the process. The snap is the resolution. The operator learns to listen for the snaps.

---

## 11. Tab Bar & Navigation (Bare Deck)

The Bare Deck Terminal (no cartridge inserted) is a **tabbed hub**. Five tabs on a persistent tab bar.

### Tab Bar

- **Position:** Row 23 (one row above the action bar).
- **Tabs:** HOME • MISSIONS • REPL • LINK • STATUS
- **Active tab:** Bracketed — `[HOME]`. Inactive tabs are plain text.
- **Navigation:** CDR cycles right through tabs (wraps HOME → MISSIONS → REPL → LINK → STATUS → HOME). CAR drills into focused content within the active tab.
- **Tab naming rule:** Tab names describe destinations, NOT key bindings. No tab shares a name with a physical key (no SYS tab, no LAMBDA tab, no CAR tab).

### Row Ownership (Bare Deck)

| Row | Content | Owner |
|-----|---------|-------|
| 0 | Status bar OR HOME content header | nOSh runtime (context-dependent) |
| 1–22 | Tab content area | Active tab |
| 23 | Tab bar | nOSh runtime |
| 24 | Action bar (key hints, per-tab) | nOSh runtime |

### Status Bar Promotion (HOME Only)

On the **HOME tab only**, Row 0's status bar info (handle, credits, time, reputation) is NOT shown in the compressed status bar format. Instead, it moves into the content area where it gets full-width treatment inside a box. On **ALL other tabs**, Row 0 displays the standard compressed status bar.

### HOME Tab Box Treatment

The HOME tab is wrapped in a full single-line box (`┌─┐│└─┘`) from Row 0 to Row 22. This is the **only screen in the system** that uses a full-page content box. Section dividers use `├──┤` between content blocks. The HOME screen is a cockpit — the box is the frame.

### Cipher Ticker (SUPERSEDED 2026-04-24)

The Row 21 main-grid Cipher ticker is retired. Cipher fragments now render on the **CIPHER-LINE OLED**, Rows 2–3, with the echo-pair behavior documented in `KN-86-UI-Design-System.md` §3A. The "always alive, never demands attention" design property is preserved — the OLED is peripheral by design. The "Legend reputation goes silent" property is preserved via the mode selector's reputation-aware bias: operators with deep cartridge_history get higher silence weight on idle beats. The Row 21 slot on the main grid HOME box is freed for other uses (e.g., extended status, bounty-preview teasers, ticker of recently completed missions — to be decided).

See `docs/software/runtime/bare-deck-terminal.md` for the full HOME tab layout, first-boot state, and section breakdowns (supersedes `docs/_archive/ui-design/KN-86-Home-Screen-Design.md`).

---

## Decisions Log

| Decision | Choice | Date |
|----------|--------|------|
| Starting approach | Design principles first, then screens | 2026-04-15 |
| Time format | Nail down now (T: + 6 hex digits) | 2026-04-15 |
| Section headers | Bare by default (Style A) | 2026-04-15 |
| Scroll region boundaries | Implicit (no boxing) | 2026-04-15 |
| Boot sequence | Full-screen scroll (Rows 0–24) | 2026-04-15 |
| Scroll history | Gone forever + Persistence Rule | 2026-04-15 |
| Sidescrolling | Yes — ticker, sweep, marquee, cascade patterns | 2026-04-15 |
| Tab bar position | Row 23, above action bar | 2026-04-16 |
| Tab navigation | CDR cycles tabs, CAR drills into content | 2026-04-16 |
| Tab naming | Destination names only — never share key names | 2026-04-16 |
| HOME box treatment | Full single-line box (only screen with this) | 2026-04-16 |
| Status bar on HOME | Promoted into content area, compressed elsewhere | 2026-04-16 |
| Cipher placement | Horizontal ticker on Row 21 inside HOME box | 2026-04-16 |
| First boot state | Shorter box, collapsed sections, static Cipher greeting | 2026-04-16 |
