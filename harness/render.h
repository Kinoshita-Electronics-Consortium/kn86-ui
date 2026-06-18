/*
 * render.h — KN-86 native framebuffer renderer (ADR-0036).
 *
 * Owns a 1024x600 RGB565 surface and draws the 8x8 KN-86 Code Page
 * (kn86_font, font.c) at integer scales (1x..Nx). The canonical display
 * surface is the panel itself (ADR-0036 §Decision): cell density is
 * per-element, not a fixed grid. A glyph at scale s occupies 8s x 8s
 * pixels.
 *
 * This is the privileged system-tier draw surface specified by ADR-0036
 * §Decision item 5 and ui-design-language.md. It is NOT the cart cell
 * API (cell_api.h) — that constrained surface is unchanged and unaffected
 * by this module.
 *
 * Colour resolution goes through phosphor.h: the foreground is the active
 * phosphor hue, the background is fixed black. An inverted pair
 * (black-on-phosphor) drives the §4 selection / focus / hazard channel.
 *
 * Backend separation: all drawing operates on a static in-process RGB565
 * surface and is pure (no SDL). render_present() / render_poll() are the
 * only SDL-touching entry points; they are compiled as live SDL3 calls
 * only when KN86_RENDER_SDL is defined (the kn86emu desktop target), and
 * as harmless stubs otherwise (unit tests, device /dev/fb0 path TBD). The
 * /dev/fb0 device backend is deferred per ADR-0036 §Decision item 8 and
 * is NOT implemented here.
 *
 * No malloc: the surface is a fixed-size static buffer. C11, snake_case
 * functions, CamelCase types.
 */

#ifndef KN86_RENDER_H
#define KN86_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include "phosphor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical surface dimensions (ADR-0036). 1024/8 = 128 cols, 600/8 = 75
 * rows is the 1x cell ceiling. */
#define RENDER_WIDTH   1024
#define RENDER_HEIGHT  600

/* Box-drawing weights for render_box(). */
typedef enum {
    RENDER_BOX_SINGLE = 0,   /* single-line frame: panels */
    RENDER_BOX_DOUBLE = 1    /* double-line frame: modals / focused panel */
} RenderBoxWeight;

/* Resolved RGB565 colour pair for a draw call. fg is the lit pixel
 * colour, bg the unlit. The renderer never mixes hues — fg/bg both come
 * from {active phosphor hue, black} (or the inverted pairing for the §4
 * selection channel). */
typedef struct {
    uint16_t fg;
    uint16_t bg;
} RenderColors;

/* ---- Surface lifecycle ---- */

/* Reset the renderer: clear the surface to black. Idempotent; safe to
 * call before any draw. Tests call this between cases. */
void render_init(void);

/* Surface dimensions (ADR-0036). */
int render_width(void);
int render_height(void);

/* Read-only pointer to the RGB565 surface (RENDER_WIDTH * RENDER_HEIGHT,
 * row-major). GWP-520: lets the live main loop blit the system-screen
 * surface into its OWN window texture without render.c opening a second
 * SDL window (render_present_init is never called on the converged path).
 * The pointer is stable for the process lifetime (static buffer); never
 * freed. */
const uint16_t *render_surface(void);

/* ---- Colour helpers (phosphor-resolved) ---- */

/* Normal pair: foreground = scheme hue, background = black. */
RenderColors render_colors_normal(PhosphorScheme scheme);

/* Inverted pair (black-on-phosphor): the §4 selection / focus / hazard
 * channel. foreground = black, background = scheme hue. */
RenderColors render_colors_inverted(PhosphorScheme scheme);

/* ---- Primitives ---- */

/* Clear the whole surface to `color` (RGB565). */
void render_clear(uint16_t color);

/* Set a single surface pixel. Out-of-bounds writes are silently dropped.
 * Used by the half-block layer and by composite.c. */
void render_set_pixel(int x, int y, uint16_t color);

/* Read a single surface pixel (0x0000 for out-of-bounds). For tests +
 * composite.c. */
uint16_t render_get_pixel(int x, int y);

/* Filled rectangle of `color`, clipped to the surface. */
void render_fill_rect(int x, int y, int w, int h, uint16_t color);

/* Blit one Code-Page glyph at integer scale. The glyph at code `ch`
 * (0..255) is read from kn86_font and drawn at top-left pixel (x, y),
 * occupying 8*scale x 8*scale pixels. Lit bits paint `colors.fg`, unlit
 * bits paint `colors.bg`. scale < 1 is treated as 1. Clipped to the
 * surface. */
void render_glyph(int x, int y, int scale, uint8_t ch, RenderColors colors);

/* Draw a string of Code-Page bytes left-to-right starting at pixel
 * (x, y), advancing 8*scale pixels per glyph. NUL-terminated; no wrap
 * (clipped at the right edge). */
void render_text(int x, int y, int scale, const char *str, RenderColors colors);

/* Draw a box (frame) of `weight` with its top-left corner at CELL
 * (col, row) in the 1x cell grid, `cw` cells wide and `ch` cells tall.
 * If `title` is non-NULL the title-cutout pattern is applied: the top
 * border cells under the title (plus a one-cell pad on each side) are
 * cleared before the title is stamped, so the border does not strike
 * through the title (ui-design-language.md §6.1). The title is drawn at
 * scale 1, offset 2 cells from the left corner. `colors` resolves the
 * line + title colour. */
void render_box(int col, int row, int cw, int ch, const char *title,
                RenderBoxWeight weight, RenderColors colors);

/* Arbitrary 1bpp bitmap blit (the font-extension / icon hook). `bits`
 * is a row-major MSB-first 1bpp bitmap `bw` px wide (rounded up to whole
 * bytes per row) and `bh` px tall, drawn at integer `scale` with its
 * top-left at pixel (x, y). Lit bits paint `colors.fg`; unlit bits are
 * left untouched (transparent) when `opaque` is false, or painted
 * `colors.bg` when `opaque` is true. Clipped to the surface. */
void render_bitmap(int x, int y, int scale, const uint8_t *bits,
                   int bw, int bh, RenderColors colors, bool opaque);

/* ---- Half-block layer (ADR-0036 §Decision item 6) ---- */

/* Light or clear a single half-block pseudo-pixel on the 128x150 canvas
 * (2 vertical sub-pixels per 1x cell). `on` non-zero lights it with the
 * active phosphor hue; zero clears it to black. Out-of-bounds dropped. */
void render_half_block_set(int hx, int hy, int on);

/* ---- Presentation / input (SDL backend; stubbed without KN86_RENDER_SDL) ---- */

/* Initialise the SDL3 window + renderer + RGB565 streaming texture. No-op
 * (returns true) when built without KN86_RENDER_SDL. Returns false on an
 * SDL init failure. */
bool render_present_init(void);

/* Push the current surface to the window. No-op without KN86_RENDER_SDL. */
void render_present(void);

/* Poll the window event queue. Returns false when a quit was requested,
 * true otherwise. Always returns true without KN86_RENDER_SDL. */
bool render_poll(void);

/* Tear down the SDL backend. No-op without KN86_RENDER_SDL. */
void render_present_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* KN86_RENDER_H */
