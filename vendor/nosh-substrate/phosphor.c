/*
 * phosphor.c — Foreground phosphor-hue switch (ADR-0036 §Decision item 7).
 *
 * Owns the AMBER / WHITE / GREEN scheme enum, the canonical RGB565
 * values, the active scheme, runtime switching, and a minimal single-key
 * `[aesthetic].mode` TOML reader/writer. See phosphor.h for the contract.
 *
 * Depends only on libc — no SDL, no display stack — so it links into both
 * the full kn86emu build and the unit tests.
 */

#include "phosphor.h"

#include <stdio.h>
#include <string.h>

/* Process-wide active scheme. AMBER is the canonical default until a
 * config load or an explicit phosphor_set() changes it. */
static PhosphorScheme g_active = PHOSPHOR_AMBER;

/* ---- Active scheme ---- */

PhosphorScheme phosphor_active(void) {
    return g_active;
}

void phosphor_set(PhosphorScheme scheme) {
    switch (scheme) {
        case PHOSPHOR_WHITE:
        case PHOSPHOR_GREEN:
            g_active = scheme;
            break;
        case PHOSPHOR_AMBER:
        default:
            g_active = PHOSPHOR_AMBER;
            break;
    }
}

uint16_t phosphor_fg_rgb565(PhosphorScheme scheme) {
    switch (scheme) {
        case PHOSPHOR_WHITE:  return PHOSPHOR_RGB565_WHITE;
        case PHOSPHOR_GREEN:  return PHOSPHOR_RGB565_GREEN;
        case PHOSPHOR_AMBER:
        default:              return PHOSPHOR_RGB565_AMBER;
    }
}

uint16_t phosphor_active_fg_rgb565(void) {
    return phosphor_fg_rgb565(g_active);
}

/* ---- Name <-> enum ---- */

PhosphorScheme phosphor_from_name(const char *name) {
    if (name == NULL) return PHOSPHOR_AMBER;
    if (strcmp(name, "amber") == 0) return PHOSPHOR_AMBER;
    if (strcmp(name, "white") == 0) return PHOSPHOR_WHITE;
    if (strcmp(name, "green") == 0) return PHOSPHOR_GREEN;
    /* Unknown / wrong-case / empty -> canonical default. */
    return PHOSPHOR_AMBER;
}

const char *phosphor_to_name(PhosphorScheme scheme) {
    switch (scheme) {
        case PHOSPHOR_WHITE:  return "white";
        case PHOSPHOR_GREEN:  return "green";
        case PHOSPHOR_AMBER:
        default:              return "amber";
    }
}

const char *phosphor_to_symbol(PhosphorScheme scheme) {
    switch (scheme) {
        case PHOSPHOR_WHITE:  return ":white";
        case PHOSPHOR_GREEN:  return ":green";
        case PHOSPHOR_AMBER:
        default:              return ":amber";
    }
}

const char *phosphor_active_symbol(void) {
    return phosphor_to_symbol(g_active);
}

/* ---- Minimal [aesthetic].mode TOML reader/writer ----
 *
 * Mirrors the read-rewrite shape of aesthetic_mode.c (ADR-0034) so the
 * on-disk contract is identical; only the accepted value set differs
 * (amber/white/green, the ADR-0036 roster). Not a general TOML parser:
 * scan for the literal `[aesthetic]` header, then the first
 * `mode = "..."` key inside it. */

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

PhosphorScheme phosphor_config_load(const char *path) {
    if (path == NULL) return PHOSPHOR_AMBER;

    FILE *f = fopen(path, "r");
    if (f == NULL) return PHOSPHOR_AMBER; /* missing file -> default */

    char line[256];
    bool in_aesthetic = false;
    PhosphorScheme result = PHOSPHOR_AMBER;
    bool found = false;

    while (fgets(line, sizeof(line), f) != NULL) {
        const char *p = skip_ws(line);

        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        if (*p == '[') {
            in_aesthetic = (strncmp(p, "[aesthetic]", 11) == 0);
            continue;
        }

        if (!in_aesthetic) continue;

        if (strncmp(p, "mode", 4) != 0) continue;
        p = skip_ws(p + 4);
        if (*p != '=') continue;
        p = skip_ws(p + 1);
        if (*p != '"') continue;
        p++; /* past opening quote */

        char value[32];
        size_t vi = 0;
        while (*p != '"' && *p != '\0' && *p != '\n' && vi < sizeof(value) - 1) {
            value[vi++] = *p++;
        }
        value[vi] = '\0';

        if (*p == '"') {
            result = phosphor_from_name(value);
            found = true;
            break;
        }
    }

    fclose(f);
    if (found) {
        g_active = result;
        return result;
    }
    return PHOSPHOR_AMBER;
}

#define PHOSPHOR_TOML_MAX (16 * 1024)

bool phosphor_config_save(const char *path) {
    if (path == NULL) return false;

    const char *name = phosphor_to_name(g_active);

    /* Slurp existing content (best-effort; missing file = empty). */
    char buf[PHOSPHOR_TOML_MAX];
    size_t len = 0;
    FILE *rf = fopen(path, "r");
    if (rf != NULL) {
        len = fread(buf, 1, sizeof(buf) - 1, rf);
        fclose(rf);
    }
    buf[len] = '\0';

    FILE *wf = fopen(path, "w");
    if (wf == NULL) return false;

    bool in_aesthetic = false;
    bool wrote_mode = false;
    bool saw_aesthetic_table = false;

    const char *cursor = buf;
    while (*cursor != '\0') {
        const char *eol = strchr(cursor, '\n');
        size_t line_len = (eol != NULL) ? (size_t)(eol - cursor) + 1
                                        : strlen(cursor);
        char line[256];
        size_t copy = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
        memcpy(line, cursor, copy);
        line[copy] = '\0';
        cursor += line_len;

        const char *p = skip_ws(line);

        if (*p == '[') {
            if (in_aesthetic && !wrote_mode) {
                fprintf(wf, "mode = \"%s\"\n", name);
                wrote_mode = true;
            }
            in_aesthetic = (strncmp(p, "[aesthetic]", 11) == 0);
            if (in_aesthetic) saw_aesthetic_table = true;
            fputs(line, wf);
            continue;
        }

        if (in_aesthetic && !wrote_mode && strncmp(p, "mode", 4) == 0) {
            const char *after = skip_ws(p + 4);
            if (*after == '=') {
                fprintf(wf, "mode = \"%s\"\n", name);
                wrote_mode = true;
                continue;
            }
        }

        fputs(line, wf);
    }

    if (in_aesthetic && !wrote_mode) {
        fprintf(wf, "mode = \"%s\"\n", name);
        wrote_mode = true;
    }
    if (!saw_aesthetic_table) {
        if (len > 0 && buf[len - 1] != '\n') fputc('\n', wf);
        fprintf(wf, "[aesthetic]\nmode = \"%s\"\n", name);
        wrote_mode = true;
    }

    fclose(wf);
    return wrote_mode;
}
