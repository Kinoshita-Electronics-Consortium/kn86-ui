/*
 * sys_context.c — System-tier KEC context creation (GWP-509).
 *
 * Assembles the System-tier binding-set (KEC Core + cell-API tier +
 * render/... tier) on a caller-owned arena. See sys_context.h for the
 * contract and runtime-arch §4b / kec-lisp-language-standard §5 for the
 * tier model: capability is the binding-set, fixed at context creation.
 *
 * The cell-API wrappers here marshal against cell_api.c (a standalone
 * module — no SDL, no termbox), so the System tier links the same
 * constrained cart draw surface the kn86-nosh-tb spike binds, without
 * depending on that target's compile flag. The render/... tier comes from
 * sys_render_bind(). A cart context would call nosh_cell_api_bind() and
 * stop; a System context additionally calls sys_render_bind() — that one
 * extra call is the entire capability difference.
 */

#include "sys_context.h"
#include "sys_render.h"
#include "cell_api.h"
#include "phosphor.h"   /* GWP-513 — aesthetic getter/setter for the SYS picker */
#include "host.h"   /* kec_bind_fe — the GC-safe bind seam (§6.1) */

#include <stdint.h>
#include <string.h>

/* ---- cell-API tier wrappers (marshal cell_api.c) ----------------- */

/* (cell-set col row ch fg bg) -> nil */
static fe_Object *lisp_cell_set(fe_Context *ctx, fe_Object *args) {
    int col = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int row = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    uint32_t ch = (uint32_t)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    cell_color_t fg = (cell_color_t)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    cell_color_t bg = (cell_color_t)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    cell_set(col, row, ch, fg, bg);
    return fe_bool(ctx, 1);
}

/* (cell-print col row fg bg str) -> nil */
static fe_Object *lisp_cell_print(fe_Context *ctx, fe_Object *args) {
    int col = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int row = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    cell_color_t fg = (cell_color_t)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    cell_color_t bg = (cell_color_t)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    char buf[CELL_API_COLS + 1];
    fe_tostring(ctx, fe_nextarg(ctx, &args), buf, (int)sizeof(buf));
    cell_print(col, row, fg, bg, buf);
    return fe_bool(ctx, 1);
}

/* (cell-clear-cart-region) -> nil */
static fe_Object *lisp_cell_clear_cart_region(fe_Context *ctx, fe_Object *args) {
    (void)args;
    cell_clear_cart_region();
    return fe_bool(ctx, 1);
}

/* (cell-cols) -> 128 */
static fe_Object *lisp_cell_cols(fe_Context *ctx, fe_Object *args) {
    (void)args;
    return fe_number(ctx, (fe_Number)cell_cols());
}

/* (cell-rows-usable) -> 73 */
static fe_Object *lisp_cell_rows_usable(fe_Context *ctx, fe_Object *args) {
    (void)args;
    return fe_number(ctx, (fe_Number)cell_rows_usable());
}

/* (half-block-set x y on) -> nil */
static fe_Object *lisp_half_block_set(fe_Context *ctx, fe_Object *args) {
    int x  = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int y  = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int on = !fe_isnil(ctx, fe_nextarg(ctx, &args));
    half_block_set(x, y, on);
    return fe_bool(ctx, 1);
}

/* (half-block-rect x y w h on) -> nil */
static fe_Object *lisp_half_block_rect(fe_Context *ctx, fe_Object *args) {
    int x  = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int y  = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int w  = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int h  = (int)fe_tonumber(ctx, fe_nextarg(ctx, &args));
    int on = !fe_isnil(ctx, fe_nextarg(ctx, &args));
    half_block_rect(x, y, w, h, on);
    return fe_bool(ctx, 1);
}

/* (half-block-clear) -> nil */
static fe_Object *lisp_half_block_clear(fe_Context *ctx, fe_Object *args) {
    (void)args;
    half_block_clear();
    return fe_bool(ctx, 1);
}

/* (present) -> nil  — advisory frame marker (cart-tier semantics). */
static fe_Object *lisp_present(fe_Context *ctx, fe_Object *args) {
    (void)args;
    present();
    return fe_bool(ctx, 1);
}

/* (yield) -> nil  — return control to the runtime. */
static fe_Object *lisp_yield(fe_Context *ctx, fe_Object *args) {
    (void)args;
    yield();
    return fe_bool(ctx, 1);
}

/* ---- aesthetic-mode tier (GWP-513 — drives phosphor.c) ----------------
 *
 * The SYS aesthetic picker (system-image/lib/sys/aesthetic.lsp) reads the
 * active phosphor scheme and cycles it. The read primitive mirrors the
 * cart-tier (get-aesthetic-mode) (nosh_lisp_bridge.c) but is sourced from
 * phosphor.c — the system context has no SystemState. The cycle primitive
 * is system-tier ONLY (carts never mutate the mode): it advances
 * EMBER -> WHITE -> GREEN -> EMBER, sets it live (recolours the surface on
 * the next frame), and persists it to nosh-config.toml [aesthetic].mode.
 */

/* The roster cycle order (ADR-0036 §Decision item 7). */
static PhosphorScheme phosphor_next(PhosphorScheme s) {
    switch (s) {
        case PHOSPHOR_EMBER: return PHOSPHOR_WHITE;
        case PHOSPHOR_WHITE: return PHOSPHOR_GREEN;
        case PHOSPHOR_GREEN:
        default:             return PHOSPHOR_EMBER;
    }
}

/* (get-aesthetic-mode) -> :ember | :white | :green
 * The active phosphor scheme as a keyword symbol (ADR-0036 amendment of
 * ADR-0034). Read-only; matches the cart-tier primitive's return contract
 * so a screen can share code across tiers. */
static fe_Object *lisp_get_aesthetic_mode(fe_Context *ctx, fe_Object *args) {
    (void)args;
    return fe_symbol(ctx, phosphor_active_symbol());
}

/* (cycle-aesthetic-mode! path) -> :ember | :white | :green
 * Advance the active scheme to the next in the roster, apply it live
 * (phosphor_set), and persist to the TOML at `path`. `path` is an optional
 * string; nil / absent uses the device default config path. Returns the new
 * active mode as a keyword symbol. System-tier only — there is no cart-tier
 * counterpart. */
static fe_Object *lisp_cycle_aesthetic_mode(fe_Context *ctx, fe_Object *args) {
    char path[512];
    const char *cfg = PHOSPHOR_CONFIG_PATH_DEFAULT;
    if (!fe_isnil(ctx, args)) {
        fe_tostring(ctx, fe_nextarg(ctx, &args), path, (int)sizeof(path));
        if (path[0] != '\0') cfg = path;
    }
    PhosphorScheme next = phosphor_next(phosphor_active());
    phosphor_set(next);
    phosphor_config_save(cfg);
    return fe_symbol(ctx, phosphor_active_symbol());
}

void nosh_aesthetic_bind(fe_Context *ctx) {
    kec_bind_fe(ctx, "get-aesthetic-mode",    lisp_get_aesthetic_mode);
    kec_bind_fe(ctx, "cycle-aesthetic-mode!", lisp_cycle_aesthetic_mode);
}

void nosh_cell_api_bind(fe_Context *ctx) {
    kec_bind_fe(ctx, "cell-set",               lisp_cell_set);
    kec_bind_fe(ctx, "cell-print",             lisp_cell_print);
    kec_bind_fe(ctx, "cell-clear-cart-region", lisp_cell_clear_cart_region);
    kec_bind_fe(ctx, "cell-cols",              lisp_cell_cols);
    kec_bind_fe(ctx, "cell-rows-usable",       lisp_cell_rows_usable);
    kec_bind_fe(ctx, "half-block-set",         lisp_half_block_set);
    kec_bind_fe(ctx, "half-block-rect",        lisp_half_block_rect);
    kec_bind_fe(ctx, "half-block-clear",       lisp_half_block_clear);
    kec_bind_fe(ctx, "present",                lisp_present);
    kec_bind_fe(ctx, "yield",                  lisp_yield);
}

/* ---- System-tier context factory --------------------------------- */

kec_State *nosh_system_context_create(void *buf, size_t size) {
    kec_State *S = kec_open_with_arena(buf, size, KEC_PROFILE_SANDBOX);
    if (S == NULL) {
        return NULL;
    }
    fe_Context *ctx = kec_fe(S);

    /* System-tier binding-set: cell-API tier + the privileged render/...
     * tier. The cart tier would stop after nosh_cell_api_bind(); the
     * extra sys_render_bind() call is the whole capability difference. */
    nosh_cell_api_bind(ctx);
    sys_render_bind(ctx);
    nosh_aesthetic_bind(ctx);   /* GWP-513 — SYS aesthetic picker glue */

    return S;
}
