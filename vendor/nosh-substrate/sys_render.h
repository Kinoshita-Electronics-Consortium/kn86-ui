/*
 * sys_render.h — System-tier render bind (GWP-509 / runtime-arch §4b).
 *
 * Binds the privileged `render/...` primitive set into a System-tier KEC
 * Lisp context. This is the COMPLEMENT of the constrained cart cell API
 * (cell_api.h): carts never get these symbols, system surfaces (board,
 * nEmacs, REPL host, SYS utils, the ui/ kit) do. Capability is the
 * binding-set, enforced at context creation (kec-lisp-language-standard
 * §2.1 / §5; runtime-arch §4b) — there is no per-call tier check.
 *
 * The 10 primitives wrap the native framebuffer renderer (render.h,
 * ADR-0036). They compose at any integer scale on the same 1024×600
 * RGB565 surface the cart tier paints, plus box-drawing, raw 1bpp blits,
 * and the present/poll loop hooks. Marshalling follows the FFI bridge
 * contract (kec-lisp-language-standard §6.2); every failure routes
 * through fe_error (§6.5), never exit(); each bind brackets the GC stack
 * (§6.1 / GWP-203).
 *
 *   (render/clear)                              clear whole framebuffer
 *   (render/text x y scale fg text)             text at integer scale
 *   (render/glyph col row code scale fg bg)     one glyph at scale
 *   (render/fill-rect x y w h fg)               solid rect (px coords)
 *   (render/box cx cy cw ch weight title?)      box-drawing frame
 *   (render/bitmap x y bytes w h fg)            arbitrary 1bpp blit
 *   (render/present)                            flush to surface
 *   (render/poll) -> :quit | nil                poll input; :quit on quit
 *   (render/width)  -> 1024                     canonical surface width
 *   (render/height) -> 600                      canonical surface height
 *
 * Colour model (render.h / phosphor.h): the renderer paints one
 * foreground hue on black. A draw call's colour pair is selected by a
 * fg token against the active phosphor scheme:
 *   fg non-nil / non-zero -> normal   (phosphor on black)
 *   fg nil / 0            -> inverted (black on phosphor; the §4
 *                            selection / focus / hazard channel)
 * render/glyph takes both fg and bg tokens and resolves the pair the
 * same way (bg picks which member is the hue). This keeps the cart-tier
 * CELL_AMBER(1)/CELL_BLACK(0) numeric convention working unchanged.
 *
 * `weight` for render/box: 0 (or :single) -> single-line, non-zero (or
 * :double) -> double-line. `title?` is optional — nil / absent draws an
 * untitled frame.
 *
 * Backend: these wrap the pure render.c surface (no SDL) plus
 * render_present()/render_poll() (SDL-backed only under KN86_RENDER_SDL,
 * harmless stubs otherwise). No malloc. C11, snake_case functions.
 */

#ifndef KN86_SYS_RENDER_H
#define KN86_SYS_RENDER_H

#include "fe.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bind the 10 privileged render/... primitives into `ctx`. Call once per
 * System-tier context, after the context's Core + cell-API tier are
 * installed. Each primitive is bound with GC-stack save/restore (the
 * kec_bind_fe seam). Carts MUST NOT be passed a context that has been
 * through this function.
 */
void sys_render_bind(fe_Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* KN86_SYS_RENDER_H */
