/*
 * sys_context.h — System-tier KEC context creation (GWP-509).
 *
 * A System-tier surface (the mission board, nEmacs, the REPL host, SYS
 * utils, and the ui/ component kit that Phase 3 builds) runs in a KEC
 * context whose binding-set is: KEC Core + the cart cell-API tier + the
 * privileged render/... tier. That binding-set IS its capability
 * (kec-lisp-language-standard §2.1 / §5; runtime-arch §4b) — a cart
 * context, by contrast, gets Core + cell-API ONLY and never sees
 * render/....
 *
 * This module is the minimal context factory that establishes the
 * System-tier binding-set so Phase 3's ui/ library has a context to run
 * in. It does NOT port any screen and does NOT own a render loop — it
 * only assembles the symbol set.
 *
 *   nosh_system_context_create(buf, size)
 *       Open a KEC context on a caller-owned arena (KEC Core loaded by
 *       kec_open_with_arena), bind the 10 cell-API builtins, then bind
 *       the 10 render/... builtins (sys_render_bind). Returns the kec_State
 *       (NULL on arena/Core-load failure). Close with kec_close.
 *
 * No malloc: the arena is caller-supplied (a static or stack buffer),
 * matching the device-friendly kec_open_with_arena contract.
 */

#ifndef KN86_SYS_CONTEXT_H
#define KN86_SYS_CONTEXT_H

#include <stddef.h>

#include "fe.h"
#include "kec.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create a System-tier KEC context on the caller-owned arena `buf` of
 * `size` bytes. Loads KEC Core (via kec_open_with_arena, SANDBOX
 * profile), then binds the System-tier binding-set: cell-API tier +
 * render/... tier. Returns the kec_State, or NULL if the arena is too
 * small to hold Core or allocation fails. The buffer must outlive the
 * returned state and is not freed by kec_close.
 */
kec_State *nosh_system_context_create(void *buf, size_t size);

/*
 * Bind ONLY the cell-API tier (the 10 cart builtins) into `ctx`. Exposed
 * so a cart-tier context factory can share the exact same cell-API
 * binding-set the System tier uses — the two tiers differ solely by
 * whether sys_render_bind is also called. Uses the GC-safe bind seam.
 */
void nosh_cell_api_bind(fe_Context *ctx);

/*
 * Bind the system-tier aesthetic-mode glue into `ctx` (GWP-513): the
 * read primitive (get-aesthetic-mode) -> :ember|:white|:green and the
 * system-only mutator (cycle-aesthetic-mode! path) that advances the
 * phosphor roster, applies it live, and persists to nosh-config.toml. Both
 * marshal against phosphor.c. Called by nosh_system_context_create; exposed
 * so a test or alternate factory can install the same glue.
 */
void nosh_aesthetic_bind(fe_Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* KN86_SYS_CONTEXT_H */
