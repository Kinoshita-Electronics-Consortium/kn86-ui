/*
 * phosphor.h — Foreground phosphor-hue switch (ADR-0036 §Decision item 7).
 *
 * The KN-86 native framebuffer renderer paints one foreground hue on a
 * fixed black background. ADR-0036 replaced the ADR-0014 "glyph
 * treatment" aesthetic axis with a *foreground phosphor colour* axis:
 * three operator-selectable schemes mapping to CRT phosphor history.
 *
 *   EMBER  #D84810 -> RGB565 0xDA42   canonical default (P3-ish).
 *   WHITE  #F0F0F0 -> RGB565 0xF79E   alternate (P4-style).
 *   GREEN  #33F033 -> RGB565 0x3FE6   alternate (P1-style).
 *
 * Background is fixed black 0x0000. EMBER is locked; WHITE/GREEN are
 * starting values pending on-glass tuning (ADR-0036 §Decision-7,
 * ui-design-language.md §12 OQ-1).
 *
 * Persistence target is nosh-config.toml `[aesthetic].mode` with the
 * value set `"ember" | "white" | "green"` — the ADR-0036 amendment of
 * ADR-0034's roster. This module owns ONLY the phosphor-scheme half of
 * that subsystem: the enum, the canonical RGB565 values, the active
 * scheme, runtime switching, and a minimal single-key TOML reader/writer.
 *
 * The Fe binding `(get-aesthetic-mode)` is deliberately NOT wired here
 * (that is GWP's follow-up story). `phosphor_active_symbol()` exposes the
 * keyword text that binding will return, and `phosphor_active()` exposes
 * the active enum, so the bridge can pick it up without re-deriving it.
 *
 * Depends only on <stdint.h>/<stdbool.h> + libc — no SDL, no display
 * stack — so it links into both the full kn86emu build and unit tests.
 */

#ifndef KN86_PHOSPHOR_H
#define KN86_PHOSPHOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The three canonical phosphor schemes (ADR-0036 §Decision item 7). */
typedef enum {
    PHOSPHOR_EMBER = 0,   /* #D84810 — canonical default */
    PHOSPHOR_WHITE = 1,   /* #F0F0F0 — P4-style alternate */
    PHOSPHOR_GREEN = 2    /* #33F033 — P1-style alternate */
} PhosphorScheme;

/* Canonical RGB565 foreground values (ADR-0036 §Decision item 7). */
#define PHOSPHOR_RGB565_EMBER  ((uint16_t)0xDA42)
#define PHOSPHOR_RGB565_WHITE  ((uint16_t)0xF79E)
#define PHOSPHOR_RGB565_GREEN  ((uint16_t)0x3FE6)

/* Fixed black background (ADR-0036 §Decision item 7). */
#define PHOSPHOR_RGB565_BLACK  ((uint16_t)0x0000)

/* Canonical config path on the device. The emulator host / tests may
 * override by passing an explicit path to the load/save helpers. */
#define PHOSPHOR_CONFIG_PATH_DEFAULT "/home/shared/nosh-config.toml"

/* ---- Active scheme (runtime-switchable) ---- */

/* The currently active scheme. Defaults to PHOSPHOR_EMBER at process
 * start (the canonical default) until phosphor_set() or a config load
 * changes it. */
PhosphorScheme phosphor_active(void);

/* Switch the active scheme. Out-of-range values clamp to EMBER. Does
 * NOT persist — call phosphor_config_save() to write the choice. */
void phosphor_set(PhosphorScheme scheme);

/* The RGB565 foreground for a scheme. Out-of-range returns the EMBER
 * value (the canonical default). */
uint16_t phosphor_fg_rgb565(PhosphorScheme scheme);

/* The RGB565 foreground for the *active* scheme. Convenience for the
 * renderer's per-frame colour resolution. */
uint16_t phosphor_active_fg_rgb565(void);

/* ---- Name <-> enum ---- */

/* Map a lowercase scheme name ("ember"|"white"|"green") to the enum.
 * Unknown / wrong-case / empty / NULL falls back to PHOSPHOR_EMBER. */
PhosphorScheme phosphor_from_name(const char *name);

/* Stable lowercase name ("ember"/"white"/"green"), for the config file.
 * Always non-NULL; out-of-range returns "ember". */
const char *phosphor_to_name(PhosphorScheme scheme);

/* The Fe keyword symbol text (":ember"/":white"/":green") that the
 * deferred (get-aesthetic-mode) binding will return. Always non-NULL. */
const char *phosphor_to_symbol(PhosphorScheme scheme);

/* The active scheme's keyword symbol text. Convenience for the bridge. */
const char *phosphor_active_symbol(void);

/* ---- Persistence (minimal [aesthetic].mode TOML, ADR-0036/0034) ---- */

/* Read `[aesthetic].mode` from the TOML file at `path` and set it as the
 * active scheme. Returns the scheme loaded (PHOSPHOR_EMBER on a missing
 * file / table / key, or an invalid value). A NULL path is treated as a
 * missing file (returns EMBER, leaves the active scheme unchanged). */
PhosphorScheme phosphor_config_load(const char *path);

/* Write the active scheme to `[aesthetic].mode` of the TOML file at
 * `path`, preserving the file's other content. Appends the table if
 * absent, inserts the key if absent, or replaces the value in place.
 * Returns true on success, false on a NULL path or I/O error. */
bool phosphor_config_save(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* KN86_PHOSPHOR_H */
