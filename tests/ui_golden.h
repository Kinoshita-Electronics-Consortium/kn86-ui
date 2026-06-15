/*
 * ui_golden.h — Golden-buffer harness for the ui/ component kit
 * (GWP-510/511/512).
 *
 * Every ui/ component test drives the same loop:
 *
 *   1. Create a System-tier KEC context (nosh_system_context_create) on a
 *      static arena — the exact binding-set (Core + cell-API + the
 *      render tier) the ui/ kit runs in.
 *   2. Load the render/ wrapper lib + every ui/ source file into that
 *      context (the same way the device loads system-image/lib).
 *   3. Eval a component form (e.g. "(ui/cbox 2 2 20 6 \"X\" 'single)").
 *   4. Assert against the in-memory render surface (render_get_pixel,
 *      render.c) — the golden buffer. The cell/glyph helpers below read a
 *      scale-1 8x8 cell straight out of kn86_font (font.c) so a test can
 *      assert "cell (cx,cy) is glyph G in the inverted pair" in one call.
 *
 * This reuses the buffer-inspection pattern test_sys_render.c established
 * (render_get_pixel + a kn86_font cell matcher) and the source-eval
 * pattern (fe_read + fe_eval over a StrReader). It is header-only so each
 * component test #includes it and gets the whole harness; the test's
 * CMake target links sys_render.c + sys_context.c + render.c + phosphor.c
 * + font.c + cell_api.c + the kec stack (no SDL).
 *
 * The phosphor scheme is pinned to EMBER for deterministic golden values;
 * the inverted pair is (black fg, EMBER bg).
 */

#ifndef KN86_UI_GOLDEN_H
#define KN86_UI_GOLDEN_H

#include "kn86_test.h"

#include "sys_context.h"
#include "render.h"
#include "phosphor.h"
#include "font.h"
#include "cell_api.h"
#include "fe.h"
#include "kec.h"
#include "host.h"

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* EMBER resolved to RGB565 (ui-design-language.md §3 / phosphor.c). The
 * golden values below are all in this hue or black. */
#define UI_EMBER 0xDA42
#define UI_BLACK 0x0000

/* One 1x cell = 8 px. */
#define UI_CELL 8

/* Arena: KEC Core + the System binding-set + the render/ + ui/ libs +
 * eval scratch. The baseline loader uses 256 KB/context; the ui/ kit adds
 * a few KB of source, so give it headroom. */
#define UI_ARENA_BYTES (512 * 1024)
static uint8_t ui_arena[UI_ARENA_BYTES];

/* --- Fe error trap (for bad-arg / capability probes) ------------------ */

static volatile int ui_fe_error_seen = 0;
static jmp_buf      ui_fe_error_jmp;

static void ui_record_fe_error(fe_Context *ctx, const char *err, fe_Object *cl)
{
    (void)ctx; (void)err; (void)cl;
    ui_fe_error_seen = 1;
    longjmp(ui_fe_error_jmp, 1);
}

/* --- Source eval helper (mirrors test_sys_render.c) ------------------- */

typedef struct { const char *src; int pos; } UiStrReader;

static char ui_str_read(fe_Context *ctx, void *udata)
{
    (void)ctx;
    UiStrReader *r = (UiStrReader *)udata;
    if (r->src[r->pos] == '\0') return '\0';
    return r->src[r->pos++];
}

/* Evaluate every top-level form in `src`; returns the last value. */
static fe_Object *ui_eval(fe_Context *ctx, const char *src)
{
    UiStrReader reader = { src, 0 };
    fe_Object *last = NULL;
    int gc_outer = fe_savegc(ctx);
    for (;;) {
        int gc = fe_savegc(ctx);
        fe_Object *form = fe_read(ctx, ui_str_read, &reader);
        if (form == NULL) { fe_restoregc(ctx, gc); break; }
        last = fe_eval(ctx, form);
        fe_restoregc(ctx, gc);
        if (last != NULL) fe_pushgc(ctx, last);
    }
    fe_restoregc(ctx, gc_outer);
    if (last != NULL) fe_pushgc(ctx, last);
    return last;
}

/* --- Library file slurp + load --------------------------------------- */

/* Slurp a file into `buf`; returns length or -1. */
static long ui_slurp(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return -1;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return (long)n;
}

/* The lib files, in dependency order. UI_LIB_DIR is baked in by CMake
 * (target_compile_definitions) to the absolute system-image/lib dir. The
 * render/ wrapper loads first (some ui/ helpers call render/center-x);
 * glyphs first among ui/ (every other ui/ file uses its constants). */
static const char *ui_lib_files[] = {
    "render.lsp",
    "ui/glyphs.lsp",
    "ui/frames.lsp",
    "ui/compositing.lsp",
    "ui/lists.lsp",
    "ui/data.lsp",
    "ui/input.lsp",
    NULL
};

/* Load the render/ + ui/ libraries into `ctx`. Asserts each file is found
 * and evals without raising. */
static void ui_load_libs(fe_Context *ctx)
{
    static char src[64 * 1024];
    for (int i = 0; ui_lib_files[i] != NULL; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", UI_LIB_DIR, ui_lib_files[i]);
        long len = ui_slurp(path, src, sizeof(src));
        if (len <= 0) {
            fprintf(stderr, "  FAIL: ui lib not found / empty: %s\n", path);
            ui_fe_error_seen = 1;
            return;
        }
        ui_eval(ctx, src);
    }
}

/* --- Harness lifecycle ------------------------------------------------ */

/* Open a fresh System context with the render surface cleared, EMBER
 * pinned, and the render/ + ui/ libs loaded. Returns the kec_State; the
 * caller closes it with kec_close. */
static kec_State *ui_open(void)
{
    cell_api_reset();
    render_init();
    phosphor_set(PHOSPHOR_EMBER);
    kec_State *S = nosh_system_context_create(ui_arena, UI_ARENA_BYTES);
    KN86_ASSERT_NOT_NULL(S);
    if (S == NULL) return NULL;
    ui_load_libs(kec_fe(S));
    return S;
}

/* --- Golden-buffer assertions ---------------------------------------- */

/* Does the scale-1 8x8 cell at pixel (px,py) match kn86_font[ch] painted
 * with the (fg,bg) RGB565 pair? */
static int ui_cell_is(int px, int py, uint8_t ch, uint16_t fg, uint16_t bg)
{
    const uint8_t *glyph = kn86_font + (size_t)ch * 8;
    for (int gy = 0; gy < 8; gy++) {
        for (int gx = 0; gx < 8; gx++) {
            uint16_t expect = (glyph[gy] & (0x80 >> gx)) ? fg : bg;
            if (render_get_pixel(px + gx, py + gy) != expect) return 0;
        }
    }
    return 1;
}

/* Same, addressed in 1x CELL coordinates (col,row -> px = col*8). */
static int ui_glyph_at(int col, int row, uint8_t ch, uint16_t fg, uint16_t bg)
{
    return ui_cell_is(col * UI_CELL, row * UI_CELL, ch, fg, bg);
}

/* Normal pair: phosphor-on-black. */
static int ui_glyph_normal(int col, int row, uint8_t ch)
{
    return ui_glyph_at(col, row, ch, UI_EMBER, UI_BLACK);
}

/* Inverted pair: black-on-phosphor (the §4 selection channel). */
static int ui_glyph_inverted(int col, int row, uint8_t ch)
{
    return ui_glyph_at(col, row, ch, UI_BLACK, UI_EMBER);
}

/* Is the whole 1x cell at (col,row) a solid fill of `color`? Used to
 * assert an inverted span's background or a cleared cell. */
static int ui_cell_solid(int col, int row, uint16_t color)
{
    int px = col * UI_CELL, py = row * UI_CELL;
    for (int gy = 0; gy < UI_CELL; gy++) {
        for (int gx = 0; gx < UI_CELL; gx++) {
            if (render_get_pixel(px + gx, py + gy) != color) return 0;
        }
    }
    return 1;
}

/* Count lit (non-black) pixels in the 1x cell at (col,row). A blank cell
 * is 0; a fully-inverted cell is 64. */
static int ui_cell_lit_count(int col, int row)
{
    int px = col * UI_CELL, py = row * UI_CELL, n = 0;
    for (int gy = 0; gy < UI_CELL; gy++) {
        for (int gx = 0; gx < UI_CELL; gx++) {
            if (render_get_pixel(px + gx, py + gy) != UI_BLACK) n++;
        }
    }
    return n;
}

#endif /* KN86_UI_GOLDEN_H */
