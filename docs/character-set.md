---
title: Character Set
description: "The KN-86 Code Page — the 8×8 glyph table the kit renders."
---

:::note
Extracted from [kn86-docs](https://kn86-deckline.com), the canonical engineering docs. ADR and cross-doc links resolve there.
:::


**Date:** 2026-04-16
**Status:** Approved — Layer 1 for immediate implementation, Layer 2 pre-hardware
**Scope:** Display subsystem, font ROM, text buffer encoding, cartridge authoring surface

---

## 1. Problem

The emulator's font system covers ASCII 32–126 (95 glyphs) using a hand-drawn 8×8 bitmap baked into `font.c`. This is insufficient:

- **No box drawing or block elements.** The UI Design System specifies `─`, `│`, `┌`, `┘`, `═`, `╔`, `█`, `░`, `▓` extensively. These characters appear in every wireframe, every screen design, and every gameplay spec. Currently, any code that writes these characters to the text buffer gets a space — the renderer silently drops anything outside ASCII 32–126.
- **No accented Latin.** The device's fiction (Kinoshita Electronics Consortium — a Japanese company) and any future localization are locked out.
- **No katakana/hiragana.** The "Kinoshita" name on the device can't be rendered in its own script.
- **No symbolic vocabulary.** Lambda (λ), currency (¤, ¥), arrows (←→↑↓), mathematical operators (≤, ≥, ≠, ∞) — all missing.
- **The existing font has no design provenance.** It's a generic bitmap. The KN-86 deserves a font with character.

## 2. Design: Press Start 2P + CP437 + Unicode Path

### Font Source

**Press Start 2P** (Google Fonts, OFL license) is the primary font. It's an 8×8 pixel font designed after the Namco arcade typeface. It covers:

- Basic Latin (93 glyphs)
- Latin-1 Supplement (96 glyphs)
- Latin Extended-A (128 glyphs)
- Greek and Coptic (74 glyphs)
- Cyrillic (96 glyphs)
- General Punctuation, Currency, Arrows, Math Operators, Geometric Shapes (~48 glyphs)
- **Total: ~555 glyphs**

**IBM Code Page 437** supplies the box drawing, block elements, and miscellaneous symbols that Press Start 2P lacks. The original IBM PC BIOS 8×8 font is effectively public domain and has been extracted/republished extensively. We take glyphs from CP437's upper 128 positions (0x80–0xFF) for:

- Box drawing light and heavy (U+2500–U+257F): `─│┌┐└┘├┤┬┴┼═║╔╗╚╝╠╣╦╩╬`
- Block elements (U+2580–U+259F): `▀▄█▌▐░▒▓`
- Miscellaneous symbols: `♠♣♥♦`, `●○`, `■□`, arrows

### Layered Implementation

#### Layer 1 — Custom 256-Glyph Page (immediate)

Expand the font from 95 to 256 glyphs. Keep `uint8_t` text buffer. This is a drop-in upgrade — same memory layout, same rendering path, just more glyphs.

**Glyph allocation (positions 0x00–0xFF):**

```
0x00–0x1F  (32 slots)   Control codes — unused for display.
                         Repurposed for custom KN-86 symbols:
                         0x01 ♠  0x02 ♣  0x03 ♥  0x04 ♦
                         0x05 ●  0x06 ○  0x07 ■  0x08 □
                         0x09 ▲  0x0A ▼  0x0B ◄  0x0C ►
                         0x0D λ  0x0E ¤  0x0F ★
                         0x10–0x1F reserved for future symbols

0x20–0x7E  (95 slots)   Standard ASCII printable characters.
                         Source: Press Start 2P.

0x7F       (1 slot)     DEL — render as full block (█) or reserved.

0x80–0x9F  (32 slots)   Box drawing — light set:
                         ─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼
                         ═ ║ ╔ ╗ ╚ ╝ ╠ ╣ ╦ ╩ ╬
                         ╌ ╎ ╴ ╵ ╶ ╷ (dashed/partial)
                         + 5 spare slots
                         Source: IBM CP437 8×8 bitmaps.

0xA0–0xAF  (16 slots)   Block elements + shading:
                         ▀ ▄ █ ▌ ▐ ░ ▒ ▓
                         ▁ ▂ ▃ ▅ ▆ ▇ (fractional blocks)
                         + 2 spare slots
                         Source: IBM CP437 8×8 bitmaps.

0xB0–0xDF  (48 slots)   Latin Extended + accented characters:
                         À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï
                         Ð Ñ Ò Ó Ô Õ Ö Ø Ù Ú Û Ü Ý Þ ß
                         à á â ã ä å æ ç è é ê ë ì í î ï
                         Source: Press Start 2P.

0xE0–0xEF  (16 slots)   Math/logic/arrows:
                         ← → ↑ ↓ ≤ ≥ ≠ ≈ ∞ ± × ÷
                         ¬ ∧ ∨ ⊕
                         Source: Press Start 2P where available,
                         hand-drawn for gaps.

0xF0–0xFF  (16 slots)   Currency + KN-86 specials:
                         ¥ £ € ₿ ¢
                         remaining 11 slots reserved for
                         cartridge-specific or future symbols.
```

**Memory impact:**

| Resource | Before (95 glyphs) | After (256 glyphs) |
|----------|--------------------|--------------------|
| Font ROM | 760 bytes | 2,048 bytes |
| Text buffer | 2,000 bytes | 2,000 bytes |
| Attribute approach | Bit 7 stolen for invert | Dedicated attribute buffer (see below) |

**Attribute buffer change:** The current code steals bit 7 of each text cell for the "inverted" flag, limiting the effective character set to 128 glyphs (0x00–0x7F). To use all 256 positions, we move the invert flag to a separate `uint8_t attrib_buffer[KN86_TEXT_COLS * KN86_TEXT_ROWS]`. Each attribute byte stores:

```c
/* Bit layout for attrib_buffer entries */
#define ATTR_INVERT  0x01   /* Inverse video */
#define ATTR_BLINK   0x02   /* Blink (future) */
#define ATTR_DIM     0x04   /* Half-brightness (future) */
/* Bits 3-7 reserved */
```

Total screen memory: 2,000 (text) + 2,000 (attrib) = 4,000 bytes. Trivially small on target hardware.

**Rendering change in `display.c`:**

```c
/* Before */
char c = state->text_buffer[idx];
bool inverted = (c & 0x80) != 0;
c = c & 0x7F;
if (c < 32 || c > 126) c = ' ';
const uint8_t *font_data = kn86_font + (c - 32) * 8;

/* After */
uint8_t c = state->text_buffer[idx];
uint8_t attr = state->attrib_buffer[idx];
bool inverted = (attr & ATTR_INVERT) != 0;
const uint8_t *font_data = kn86_font + c * 8;
```

The renderer becomes simpler — no masking, no range check, direct index. Every byte value 0–255 maps to a glyph.

**Cartridge authoring surface:** Cartridge C code uses `nosh_text_putc()` with the KN-86 code page values. For readability, `nosh_cart.h` provides named constants:

```c
#define KN86_BOX_H       0x80   /* ─ horizontal line */
#define KN86_BOX_V       0x81   /* │ vertical line */
#define KN86_BOX_TL      0x82   /* ┌ top-left corner */
#define KN86_BOX_TR      0x83   /* ┐ top-right corner */
#define KN86_BOX_BL      0x84   /* └ bottom-left corner */
#define KN86_BOX_BR      0x85   /* ┘ bottom-right corner */
#define KN86_BLOCK_FULL  0xA2   /* █ full block */
#define KN86_BLOCK_LIGHT 0xA5   /* ░ light shade */
#define KN86_BLOCK_MED   0xA6   /* ▒ medium shade */
#define KN86_BLOCK_DARK  0xA7   /* ▓ dark shade */
#define KN86_LAMBDA      0x0D   /* λ */
#define KN86_CREDITS     0x0E   /* ¤ */
/* ... etc */
```

A helper for box drawing:

```c
void nosh_draw_box(uint8_t col, uint8_t row, uint8_t w, uint8_t h);
void nosh_draw_hline(uint8_t col, uint8_t row, uint8_t len);
void nosh_draw_vline(uint8_t col, uint8_t row, uint8_t len);
```

#### Layer 2 — Unicode Subset (pre-hardware, future sprint)

Change `text_buffer` from `uint8_t` to `uint16_t`. Each cell holds a Unicode codepoint. The font table becomes a sparse lookup covering a curated subset of ~1,500–2,000 glyphs.

**What Layer 2 adds beyond Layer 1:**

- **Katakana** (U+30A0–U+30FF, 96 glyphs) — "キノシタ" on the boot screen
- **Hiragana** (U+3040–U+309F, 83 glyphs) — fiction layer flavor text
- **Full box drawing block** (U+2500–U+257F, 128 glyphs) — every variant
- **Full block elements** (U+2580–U+259F, 32 glyphs) — every fractional block
- **Braille patterns** (U+2800–U+28FF) — potential high-res text-mode graphics (each cell encodes 2×4 dot pattern = 8 sub-pixels per character cell, effectively doubling resolution)
- **Additional Cyrillic, Greek** — whatever Press Start 2P provides
- **Expanded math/technical** symbols

**Memory footprint:**

| Resource | Layer 1 (256) | Layer 2 (~2,000) |
|----------|---------------|-------------------|
| Text buffer | 2,000 bytes (uint8_t) | 4,000 bytes (uint16_t) |
| Attrib buffer | 2,000 bytes | 2,000 bytes |
| Font ROM | 2 KB | 16 KB |
| Glyph lookup | Direct index O(1) | Binary search O(log n) on sorted codepoint table |
| **Total** | **~6 KB** | **~22 KB** |
| Share of Pi Zero 2 W 512 MB RAM | < 0.01% | < 0.01% |

22KB is comfortable. The font ROM is read-only and could live in flash (4MB) rather than RAM on the production device, bringing RAM cost down to ~6KB regardless of glyph count.

**Text input:** The KN-86's 31-key keyboard uses token/grammar-based input through the REPL and nEmacs. Operators select Lisp tokens, not individual characters. Unicode characters in strings are authored in cartridge C source (which is UTF-8) and rendered by the display system — no character-by-character input method is needed on the device.

**API change:** `nosh_text_puts()` stays `const char *` (UTF-8 C strings). The puts function decodes UTF-8 and writes `uint16_t` codepoints to the text buffer. Cartridge code doesn't change:

```c
/* This Just Works in Layer 2 — UTF-8 decoded to uint16_t cells */
nosh_text_puts(2, 2, "キノシタ KN-86 DECKLINE");
nosh_text_puts(2, 3, "Ñoño señor über — Кириллица");
```

**Glyph lookup implementation:**

```c
typedef struct {
    uint16_t codepoint;     /* Unicode codepoint */
    uint16_t glyph_index;   /* Index into font bitmap array */
} GlyphEntry;

/* Sorted by codepoint for binary search */
static const GlyphEntry glyph_table[GLYPH_COUNT] = { ... };

static const uint8_t *lookup_glyph(uint16_t codepoint) {
    /* Binary search — O(log 2000) ≈ 11 comparisons */
    int lo = 0, hi = GLYPH_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (glyph_table[mid].codepoint == codepoint) {
            return kn86_font + glyph_table[mid].glyph_index * 8;
        }
        if (glyph_table[mid].codepoint < codepoint) lo = mid + 1;
        else hi = mid - 1;
    }
    return kn86_font; /* Fallback: glyph 0 (replacement character) */
}
```

11 comparisons per character × 2,000 characters per screen = 22,000 comparisons per full redraw. Sub-millisecond on target hardware. Not a concern.

**Layer 2 does NOT require Layer 1 to be thrown away.** The 256-glyph page from Layer 1 maps directly to Unicode codepoints 0x00–0xFF (Latin-1). The same bitmap data is reused. Layer 2 just widens the addressing and adds more glyphs to the table.

---

## 3. Font Asset Pipeline

### Layer 1 Pipeline

1. **Extract Press Start 2P bitmaps.** The font ships as a TrueType file. Render each codepoint at 8px into an 8×8 bitmap and export as a C array. Tool: Python script with Pillow (`pip install Pillow`), render each glyph to an 8×8 image, read pixels, pack into bytes MSB-first.

2. **Extract CP437 box drawing bitmaps.** Source: any of the widely-available IBM PC BIOS 8×8 font dumps (e.g., `int10h.org` Perfect DOS VGA 437 font, or the raw BIOS data). The box drawing glyphs at CP437 positions 0xB3–0xDA and block elements at 0xB0–0xB2, 0xDB–0xDF map to our custom page positions.

3. **Hand-draw missing glyphs.** For any positions not covered by Press Start 2P or CP437 (λ, ¤, ★, currency symbols, etc.), draw 8×8 bitmaps by hand. This is 10–20 glyphs.

4. **Assemble into `font.c`.** A build-time script reads the glyph sources and outputs `const uint8_t kn86_font[256 * 8]`.

### Layer 2 Pipeline (future)

Same approach, but the output is a larger table with a codepoint index. Katakana and hiragana glyphs can be sourced from open-source 8×8 Japanese bitmap fonts (e.g., k8x12 or misaki font, both freely licensed).

---

## 4. `.kn86demo` Format Impact

The device video playback spike (`docs/spikes/device-video-playback-spike.md`) defines `.kn86demo` frames as delta-encoded character data at 1 byte per cell. Layer 1 is compatible — the byte is now a KN-86 code page value instead of ASCII, but the format is unchanged.

Layer 2 changes the cell width to 2 bytes per character in `.kn86demo` frames. The format version byte should be bumped (v1 = uint8_t cells, v2 = uint16_t cells). Delta encoding still works identically — just wider entries.

---

## 5. Marketing Site & Attract Mode

For the Next.js deckline site and the Remotion attract mode pipeline, Press Start 2P is used as a web font via Google Fonts:

```css
@import url('https://fonts.googleapis.com/css2?family=Press+Start+2P&display=swap');

.kn86-terminal {
    font-family: 'Press Start 2P', monospace;
    font-size: 8px; /* or multiples: 16px, 24px, 32px */
    color: #E6A020; /* AMBER — canonical default per ADR-0036 */
    background: #000;
}
```

Box drawing characters render natively in browsers via Unicode — no special handling needed.

---

## 6. Documents Affected

This design supersedes or modifies the following:

| Document | Section | Change |
|----------|---------|--------|
| `kn86-emulator/src/font.h` | Header comment | Will change from "ASCII 32-126 (95 characters)" to "KN-86 Code Page (256 glyphs)" |
| `kn86-emulator/src/font.c` | Entire file | Replaced with 256-glyph table |
| `kn86-emulator/src/display.c` | Text rendering loop (line ~210) | Remove bit-7 invert hack, use attrib_buffer, direct glyph index |
| `kn86-emulator/src/types.h` | SystemState struct | Add `attrib_buffer[KN86_TEXT_COLS * KN86_TEXT_ROWS]` |
| `docs/_archive/hardware/KN-86-Modern-Build-Specification.md` (archived 2026-04-21; line refs preserved) | §Display Manager, line ~589–611 | Update "custom amber-on-black bitmap font" to reference Press Start 2P. Update "Characters align to an 8×8 grid" to note Layer 2 Unicode path. Add KN-86 Code Page reference. |
| `docs/_archive/hardware/KN-86-Modern-Build-Specification.md` (archived 2026-04-21; line refs preserved) | §Display API, line ~618 | `nosh_text_putc` signature: note `uint8_t c` maps to KN-86 code page (Layer 1) or Unicode codepoint via `nosh_text_putwc(uint16_t c)` (Layer 2) |
| `docs/_archive/hardware/KN-86-Modern-Build-Specification.md` (archived 2026-04-21; line refs preserved) | §Memory Map, line ~871 | "fonts" allocation: update from generic to "Press Start 2P + CP437 box drawing, 2KB (Layer 1) or 16KB (Layer 2)" |
| `docs/_archive/spikes/device-video-playback-spike.md` (archived 2026-04-21; line refs preserved) | §Format Sketch, line ~63 | Note: `char: 1 byte` applies to v1 format (Layer 1). v2 format uses `uint16_t` cells. |
| `docs/software/runtime/prototype-architecture.md` | Line ~182 | "128-character bitmap atlas" → "256-glyph KN-86 Code Page (Layer 1) or ~2,000-glyph Unicode subset (Layer 2)" |
| `docs/software/runtime/prototype-architecture.md` | Line ~282–283 | `font_8x8.bin` naming is fine. Note: contents change from ASCII to KN-86 Code Page. |
| `CLAUDE.md` | Canonical Hardware Spec table | Add row: "Character set: KN-86 Code Page (256 glyphs, Layer 1). Unicode subset (~2,000 glyphs, Layer 2 planned). Font: Press Start 2P + CP437 box drawing at 8×8 bitmap." |

---

## 7. Implementation Order

**Layer 1 (this sprint):**

1. Write Python extraction script for Press Start 2P → 8×8 bitmaps → C array
2. Source CP437 box drawing + block element bitmaps (8×8)
3. Hand-draw λ, ¤, ★, and other custom symbols at 8×8
4. Assemble `font.c` with 256 glyphs
5. Add `attrib_buffer` to `SystemState` in `types.h`
6. Update `display.c` renderer to use new glyph table + attrib_buffer
7. Update `nosh_stdlib.c` draw helpers to use `KN86_BOX_*` constants
8. Add `KN86_BOX_*` / `KN86_BLOCK_*` constants to `nosh_cart.h`
9. Update `bare_deck.c` to use box drawing constants instead of ASCII `-`, `|`, `+`
10. Update cartridges to use box drawing constants
11. Run full test suite, visual verification

**Layer 2 (pre-hardware sprint):**

1. Change `text_buffer` from `uint8_t` to `uint16_t`
2. Add UTF-8 decoder to `nosh_text_puts()`
3. Implement sparse glyph lookup (binary search)
4. Source katakana/hiragana 8×8 bitmaps (open-source Japanese bitmap font)
5. Expand glyph table to ~2,000 entries
6. Bump `.kn86demo` format version
7. Update marketing site to use Press Start 2P from Google Fonts

---

## 8. Risks

| Risk | Mitigation |
|------|------------|
| Press Start 2P glyphs look wrong at exactly 8×8 (it's designed for multiples of 8, may need 16px for proper rendering) | Render at 8px, visually compare. If bad, render at 16px and downscale 2×. The font's GitHub has bitmap specimens. |
| CP437 box drawing style clashes with Press Start 2P letterforms | Both are 8×8 pixel fonts from the same era. Should harmonize naturally. Hand-adjust any outliers. |
| Layer 2 binary search is too slow for target hardware | At 22,000 comparisons per full redraw with 16-bit compares, this is sub-millisecond on Pi Zero 2 W. Not a concern. Could also use a 256-entry page cache for hot glyphs. |
| Font license issue | Press Start 2P is OFL 1.1. CP437 BIOS font is public domain. Both allow embedding. |

---

## 9. Extension: KN-86-2026-06-13 (icons + bar/meter glyphs)

**Date:** 2026-06-13
**Status:** Approved — codepoints + bitmaps canonical; `font.c` regen via `tools/gen_font.py` tracked as a separate engineering follow-up.

This extension adds 21 new glyphs to the Layer 1 KN-86 Code Page, drawn from the 2026-06-13 on-glass design session against the `deckline` prototype. The new glyphs slot into three previously-reserved ranges and tighten the reservation policy in each one:

- **0x10–0x1F — KN-86 icons.** Range originally reserved for "future symbols" (see §2 Layer 1 map). Formally re-reserved here as "KN-86 icons". 4 defined (0x10–0x13); 12 reserved (0x14–0x1F) for future icon work.
- **0x96–0x9F — bar/meter glyphs (vertical).** Range was an unallocated tail inside the 0x80–0x9F box-drawing block. Formally reserved here as "bar/meter glyphs — vertical". 8 defined (0x96–0x9D, an 8-step `▁`→`█` ramp); 2 reserved (0x9E–0x9F).
- **0xA8–0xAF — bar/meter glyphs (horizontal).** Range was an unallocated tail inside the 0xA0–0xAF block-elements block. Formally reserved here as "bar/meter glyphs — horizontal". 7 defined (0xA8–0xAE, left-fill 1/8 .. 7/8); 1 reserved (0xAF). The 8/8 case is served by full-block 0xA2.

### 9.1 Reserved ranges

| Range | Purpose | Defined | Reserved |
|-------|---------|---------|----------|
| 0x10–0x1F | KN-86 icons | 0x10–0x13 (4) | 0x14–0x1F (12) — future icons |
| 0x96–0x9F | Bar/meter glyphs — vertical | 0x96–0x9D (8) — `▁`→`█` 1/8 .. 8/8 ramp | 0x9E–0x9F (2) |
| 0xA8–0xAF | Bar/meter glyphs — horizontal | 0xA8–0xAE (7) — left-fill 1/8 .. 7/8 | 0xAF (1); 8/8 = 0xA2 (full-block) |

### 9.2 KN-86 icons (0x10–0x13)

8×8 bitmaps, MSB-first. Bit 7 = leftmost pixel; row 0 = topmost row.

| Code | Name | Bitmap bytes | Description |
|------|------|--------------|-------------|
| 0x10 | CARTRIDGE | `7E 5A 5A 42 42 7E 24 00` | Cart shell with label window (rows 1–2) and contact legs (row 6). |
| 0x11 | BATTERY | `18 7E 42 5A 5A 5A 7E 00` | Battery nub (row 0) + body with three charge bars (rows 3–5). |
| 0x12 | SIGNAL | `00 00 00 01 05 15 55 00` | Four ascending signal-strength bars, anchored to the baseline. |
| 0x13 | PADLOCK | `3C 42 42 7E 66 66 7E 00` | Shackle (rows 0–2), body (rows 3–6) with keyhole gap; marks encrypted files. |

### 9.3 Bar/meter glyphs — vertical ramp (0x96–0x9D)

8-step ramp from `▁` (1/8 cell, bottom) to `█` (full block). Each glyph k (1..8) is `[0x00] * (8 - k) + [0xFF] * k` — k filled rows anchored to the bottom of the cell.

| Code | Name | Bitmap bytes | Fill |
|------|------|--------------|------|
| 0x96 | BAR_V_1_8 | `00 00 00 00 00 00 00 FF` | 1/8 |
| 0x97 | BAR_V_2_8 | `00 00 00 00 00 00 FF FF` | 2/8 |
| 0x98 | BAR_V_3_8 | `00 00 00 00 00 FF FF FF` | 3/8 |
| 0x99 | BAR_V_4_8 | `00 00 00 00 FF FF FF FF` | 4/8 |
| 0x9A | BAR_V_5_8 | `00 00 00 FF FF FF FF FF` | 5/8 |
| 0x9B | BAR_V_6_8 | `00 00 FF FF FF FF FF FF` | 6/8 |
| 0x9C | BAR_V_7_8 | `00 FF FF FF FF FF FF FF` | 7/8 |
| 0x9D | BAR_V_8_8 | `FF FF FF FF FF FF FF FF` | 8/8 (full block) |

The 8/8 case (0x9D) overlaps with full-block 0xA2 — both render an identical glyph. The duplicate is intentional: it keeps the ramp index arithmetic (`0x96 + (k - 1)`) closed across the full range so callers don't have to special-case the top step.

### 9.4 Bar/meter glyphs — horizontal sub-cell bars (0xA8–0xAE)

Left-fill 1/8 .. 7/8 cell width. Each glyph k (1..7) is `[(0xFF << (8 - k)) & 0xFF] * 8` — every row carries the same `k`-column-wide left-justified bar.

| Code | Name | Bitmap byte (×8 rows) | Fill |
|------|------|-----------------------|------|
| 0xA8 | BAR_H_1_8 | `80` | 1/8 left |
| 0xA9 | BAR_H_2_8 | `C0` | 2/8 left |
| 0xAA | BAR_H_3_8 | `E0` | 3/8 left |
| 0xAB | BAR_H_4_8 | `F0` | 4/8 left |
| 0xAC | BAR_H_5_8 | `F8` | 5/8 left |
| 0xAD | BAR_H_6_8 | `FC` | 6/8 left |
| 0xAE | BAR_H_7_8 | `FE` | 7/8 left |

The 8/8 case is served by full-block 0xA2 (`FF` × 8 rows) — no separate codepoint is allocated here.

### 9.5 Usage notes

**Sparkline recipe (logradar).** Maintain a 24-bucket rolling window of the metric. Each frame, compute the non-zero mean of the window and set the soft cap at 3× that mean (clamps a single outlier from flattening the whole strip). Map each bucket linearly into the 0x96–0x9D ramp: `glyph = 0x96 + clamp(floor(value / cap * 8), 0, 7)`. Bold the newest (rightmost) bucket via the standard invert attribute. Empty buckets render as space (0x20), not 0x96 — keep the baseline clean.

**Fine progress bar recipe.** For a bar of N cells representing a fraction `frac` in [0, 1]:

```
filled_cells   = floor(N * frac)
sub_eighths    = floor((N * frac - filled_cells) * 8)   /* 0..7 */
remaining      = N - filled_cells - (sub_eighths > 0 ? 1 : 0)

render: full-block(0xA2) × filled_cells
     || (sub_eighths > 0 ? BAR_H_{sub_eighths}_8(0xA7 + sub_eighths) : "")
     || light-shade(0xA5) × remaining
```

This produces `█████▊░░░ 78%` style bars with sub-cell precision. The partial-cell glyph is `0xA8 + (sub_eighths - 1)` — equivalently `0xA7 + sub_eighths` for `sub_eighths` in 1..7. Light-shade 0xA5 is the background convention from §2 Layer 1; substitute a different background glyph if the surface calls for it.

**Icons as inline UI annotations.** 0x10–0x13 are intended for inline UI markers inside the firmware-rendered rows (status bar row 0, action bar row 74) and inside cartridge UI chrome — file-list encryption marker (0x13 PADLOCK), battery/charge indicator (0x11 BATTERY), link-quality indicator (0x12 SIGNAL), cartridge-state indicator (0x10 CARTRIDGE). They are not narrative glyphs; CIPHER and other voice surfaces should not consume them as expressive vocabulary.

### 9.6 Validation reference

Designed and validated on-glass against the `deckline` prototype, 2026-06-13. Test utility preserved at `/home/kn86/fb_restest.py` on the device; `python3 ~/fb_restest.py glyphs` renders the showcase. Implementation in `kn86-emulator/src/font.c` via `tools/gen_font.py` is a separate engineering follow-up — this section establishes the canonical codepoints + bitmaps.

The same on-glass session also produced [ADR-0036: Native framebuffer renderer](../../../adr/ADR-0036-native-framebuffer-renderer.md), which superseded the termbox2 display layer. The glyph work here is a sibling artifact of that decision — both were exercised through the same `fb_restest.py` harness on the prototype.
