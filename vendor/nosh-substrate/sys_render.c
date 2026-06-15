/*
 * sys_render.c — System-tier render bind (GWP-509 / runtime-arch §4b).
 *
 * Wraps the native framebuffer renderer (render.c, ADR-0036) as the
 * privileged `render/...` KEC Lisp primitive set and binds it into a
 * System-tier context. See sys_render.h for the surface contract and the
 * colour model.
 *
 * Marshalling follows kec-lisp-language-standard §6.2:
 *   - Number  : fe_tonumber -> long/int (single-precision float source;
 *               render coords are well inside ±2^24).
 *   - String  : fe_tostring into a fixed scratch buffer (immutable Fe
 *               strings; NUL-terminated, clamped on overflow).
 *   - Bytes   : fe_tostring into a packed buffer (1bpp payload), budget
 *               (w*h+7)/8 enforced per the §6.2 Bytes row.
 *   - Bool    : test !fe_isnil for the fg / bg colour tokens.
 *   - Unit    : side-effecting primitives return fe_bool(ctx, 1) — the
 *               truthy-nil sentinel used across the bridge.
 *
 * Error propagation (§6.5): a bad-argument primitive raises via
 * fe_error BEFORE mutating the surface; it never exit()s and never
 * leaves a half-applied draw. The runtime's installed handler recovers.
 *
 * Colour resolution (sys_render.h): fg / bg tokens select a RenderColors
 * pair against the active phosphor scheme (phosphor_active()):
 *   fg non-nil/non-zero -> normal   (hue on black)
 *   fg nil/0            -> inverted (black on hue; the §4 channel)
 */

#include "sys_render.h"
#include "render.h"
#include "phosphor.h"
#include "host.h"   /* kec_bind_fe — the GC-safe bind seam (§6.1) */

#include <stdint.h>
#include <string.h>

/* ---- Argument marshalling helpers -------------------------------- */

/*
 * Pull the next arg as an int. fe_tonumber on a non-number traps via
 * Fe's own type machinery (fe_error -> installed handler), which is the
 * §6.5 path. Render coordinates fit an int with room to spare.
 */
static int sr_arg_int(fe_Context *ctx, fe_Object **args) {
    return (int)fe_tonumber(ctx, fe_nextarg(ctx, args));
}

/*
 * Resolve a colour token to a 1/0 selector: 1 = use the phosphor hue
 * here (normal-foreground intent), 0 = use black here. A non-nil,
 * non-zero token reads as the hue; nil or 0 reads as black. This keeps
 * the cart-tier CELL_AMBER(1)/CELL_BLACK(0) numeric convention intact
 * while also accepting bare truthy/nil tokens.
 */
static int sr_color_is_lit(fe_Context *ctx, fe_Object *obj) {
    if (fe_isnil(ctx, obj)) {
        return 0;
    }
    if (fe_type(ctx, obj) == FE_TNUMBER) {
        return fe_tonumber(ctx, obj) != 0 ? 1 : 0;
    }
    /* Any other non-nil token (a symbol such as 'phosphor) reads as
     * lit. */
    return 1;
}

/*
 * Build the RenderColors pair for a (fg, bg) token pair against the
 * active phosphor scheme. `fg_lit` true paints the hue as foreground;
 * `bg_lit` true paints the hue as background. The renderer only ever
 * mixes {hue, black}, so each member is hue when lit, black otherwise.
 */
static RenderColors sr_colors(int fg_lit, int bg_lit) {
    uint16_t hue = phosphor_active_fg_rgb565();
    RenderColors c;
    c.fg = fg_lit ? hue : PHOSPHOR_RGB565_BLACK;
    c.bg = bg_lit ? hue : PHOSPHOR_RGB565_BLACK;
    return c;
}

/* Single-token foreground pair (fg on black, or inverted when fg is
 * black/nil). Used by text / fill-rect / bitmap, which take one fg. */
static RenderColors sr_colors_fg(fe_Context *ctx, fe_Object *fg_obj) {
    int lit = sr_color_is_lit(ctx, fg_obj);
    /* fg lit  -> hue on black (normal).
     * fg unlit -> black on hue (inverted selection channel). */
    return sr_colors(lit, !lit);
}

/* ---- The 10 render/... primitives ---------------------------------- */

/* (render/clear) -> nil  — clear the whole framebuffer to black. */
static fe_Object *lisp_render_clear(fe_Context *ctx, fe_Object *args) {
    (void)args;
    render_clear(PHOSPHOR_RGB565_BLACK);
    return fe_bool(ctx, 1);
}

/* (render/text x y scale fg text) -> nil */
static fe_Object *lisp_render_text(fe_Context *ctx, fe_Object *args) {
    int x     = sr_arg_int(ctx, &args);
    int y     = sr_arg_int(ctx, &args);
    int scale = sr_arg_int(ctx, &args);
    fe_Object *fg_obj = fe_nextarg(ctx, &args);
    char buf[RENDER_WIDTH / 8 + 1]; /* max glyphs across the surface + NUL */
    fe_tostring(ctx, fe_nextarg(ctx, &args), buf, (int)sizeof(buf));

    if (scale < 1) {
        fe_error(ctx, "render/text: scale must be >= 1");
        return fe_bool(ctx, 1); /* unreachable — fe_error unwinds */
    }
    RenderColors c = sr_colors_fg(ctx, fg_obj);
    render_text(x, y, scale, buf, c);
    return fe_bool(ctx, 1);
}

/* (render/glyph col row code scale fg bg) -> nil
 *
 * col/row are CELL coordinates on the 1× grid; the renderer takes pixel
 * coordinates, so multiply by 8. */
static fe_Object *lisp_render_glyph(fe_Context *ctx, fe_Object *args) {
    int col   = sr_arg_int(ctx, &args);
    int row   = sr_arg_int(ctx, &args);
    int code  = sr_arg_int(ctx, &args);
    int scale = sr_arg_int(ctx, &args);
    fe_Object *fg_obj = fe_nextarg(ctx, &args);
    fe_Object *bg_obj = fe_nextarg(ctx, &args);

    if (scale < 1) {
        fe_error(ctx, "render/glyph: scale must be >= 1");
        return fe_bool(ctx, 1);
    }
    if (code < 0 || code > 255) {
        fe_error(ctx, "render/glyph: code must be 0..255");
        return fe_bool(ctx, 1);
    }
    RenderColors c = sr_colors(sr_color_is_lit(ctx, fg_obj),
                               sr_color_is_lit(ctx, bg_obj));
    render_glyph(col * 8, row * 8, scale, (uint8_t)code, c);
    return fe_bool(ctx, 1);
}

/* (render/fill-rect x y w h fg) -> nil */
static fe_Object *lisp_render_fill_rect(fe_Context *ctx, fe_Object *args) {
    int x = sr_arg_int(ctx, &args);
    int y = sr_arg_int(ctx, &args);
    int w = sr_arg_int(ctx, &args);
    int h = sr_arg_int(ctx, &args);
    fe_Object *fg_obj = fe_nextarg(ctx, &args);

    if (w < 0 || h < 0) {
        fe_error(ctx, "render/fill-rect: w and h must be >= 0");
        return fe_bool(ctx, 1);
    }
    /* fill-rect paints a single solid colour: the hue when fg is lit,
     * black when fg is black/nil. */
    uint16_t color = sr_color_is_lit(ctx, fg_obj)
                         ? phosphor_active_fg_rgb565()
                         : PHOSPHOR_RGB565_BLACK;
    render_fill_rect(x, y, w, h, color);
    return fe_bool(ctx, 1);
}

/* (render/box cx cy cw ch weight title?) -> nil
 *
 * cx/cy/cw/ch are CELL coordinates. weight: 0 / nil / :single ->
 * single-line, non-zero / :double -> double-line. title? optional. */
static fe_Object *lisp_render_box(fe_Context *ctx, fe_Object *args) {
    int cx = sr_arg_int(ctx, &args);
    int cy = sr_arg_int(ctx, &args);
    int cw = sr_arg_int(ctx, &args);
    int ch = sr_arg_int(ctx, &args);
    fe_Object *weight_obj = fe_nextarg(ctx, &args);
    fe_Object *title_obj  = fe_nextarg(ctx, &args);

    if (cw < 2 || ch < 2) {
        fe_error(ctx, "render/box: cw and ch must be >= 2");
        return fe_bool(ctx, 1);
    }

    RenderBoxWeight weight = sr_color_is_lit(ctx, weight_obj)
                                 ? RENDER_BOX_DOUBLE
                                 : RENDER_BOX_SINGLE;

    char title_buf[RENDER_WIDTH / 8 + 1];
    const char *title = NULL;
    if (!fe_isnil(ctx, title_obj)) {
        fe_tostring(ctx, title_obj, title_buf, (int)sizeof(title_buf));
        title = title_buf;
    }

    RenderColors c = render_colors_normal(phosphor_active());
    render_box(cx, cy, cw, ch, title, weight, c);
    return fe_bool(ctx, 1);
}

/* Worst-case 1bpp payload: the whole surface (1024*600/8 bytes). */
#define SYS_RENDER_BITMAP_MAX_BYTES ((RENDER_WIDTH * RENDER_HEIGHT) / 8)

/* (render/bitmap x y bytes w h fg) -> nil
 *
 * `bytes` is a Fe string carrying the packed 1bpp payload (row-major,
 * MSB-first). Budget (w*h+7)/8; a short payload raises per §6.2/§6.5.
 * Drawn opaque (lit bits = hue, unlit = black) so it composes like a
 * sprite blit at scale 1. */
static fe_Object *lisp_render_bitmap(fe_Context *ctx, fe_Object *args) {
    int x = sr_arg_int(ctx, &args);
    int y = sr_arg_int(ctx, &args);
    fe_Object *bytes_obj = fe_nextarg(ctx, &args);
    int w = sr_arg_int(ctx, &args);
    int h = sr_arg_int(ctx, &args);
    fe_Object *fg_obj = fe_nextarg(ctx, &args);

    if (w < 0 || h < 0) {
        fe_error(ctx, "render/bitmap: w and h must be >= 0");
        return fe_bool(ctx, 1);
    }

    static uint8_t bits[SYS_RENDER_BITMAP_MAX_BYTES + 1];
    int copied = fe_tostring(ctx, bytes_obj, (char *)bits, (int)sizeof(bits));
    long required = ((long)w * h + 7) / 8;
    if ((long)copied < required) {
        fe_error(ctx, "render/bitmap: payload shorter than (w*h+7)/8");
        return fe_bool(ctx, 1);
    }

    RenderColors c = sr_colors_fg(ctx, fg_obj);
    render_bitmap(x, y, 1, bits, w, h, c, true);
    return fe_bool(ctx, 1);
}

/* (render/present) -> nil  — flush the surface to the host backend. */
static fe_Object *lisp_render_present(fe_Context *ctx, fe_Object *args) {
    (void)args;
    render_present();
    return fe_bool(ctx, 1);
}

/* (render/poll) -> :quit | nil  — poll the host event queue. Returns the
 * :quit keyword when the host requested a quit, nil otherwise. */
static fe_Object *lisp_render_poll(fe_Context *ctx, fe_Object *args) {
    (void)args;
    bool running = render_poll();
    if (!running) {
        return fe_symbol(ctx, ":quit");
    }
    return fe_bool(ctx, 0); /* nil */
}

/* (render/width) -> 1024 */
static fe_Object *lisp_render_width(fe_Context *ctx, fe_Object *args) {
    (void)args;
    return fe_number(ctx, (fe_Number)render_width());
}

/* (render/height) -> 600 */
static fe_Object *lisp_render_height(fe_Context *ctx, fe_Object *args) {
    (void)args;
    return fe_number(ctx, (fe_Number)render_height());
}

/* ---- Registration ------------------------------------------------ */

void sys_render_bind(fe_Context *ctx) {
    /* kec_bind_fe brackets each bind with fe_savegc/fe_restoregc around
     * the symbol intern + cfunc wrap (GWP-203 / §6.1). */
    kec_bind_fe(ctx, "render/clear",     lisp_render_clear);
    kec_bind_fe(ctx, "render/text",      lisp_render_text);
    kec_bind_fe(ctx, "render/glyph",     lisp_render_glyph);
    kec_bind_fe(ctx, "render/fill-rect", lisp_render_fill_rect);
    kec_bind_fe(ctx, "render/box",       lisp_render_box);
    kec_bind_fe(ctx, "render/bitmap",    lisp_render_bitmap);
    kec_bind_fe(ctx, "render/present",   lisp_render_present);
    kec_bind_fe(ctx, "render/poll",      lisp_render_poll);
    kec_bind_fe(ctx, "render/width",     lisp_render_width);
    kec_bind_fe(ctx, "render/height",    lisp_render_height);
}
