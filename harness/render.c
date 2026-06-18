/*
 * render.c — KN-86 native framebuffer renderer (ADR-0036).
 *
 * Owns a static 1024x600 RGB565 surface and draws the 8x8 KN-86 Code
 * Page (kn86_font, font.c) at integer scales. All drawing is pure (no
 * SDL): primitives operate on the in-process surface. The SDL3 desktop
 * backend (present / poll) is compiled in only when KN86_RENDER_SDL is
 * defined; otherwise those entry points are harmless stubs so the unit
 * tests link without SDL.
 *
 * No malloc: the surface is a fixed-size static buffer. C11.
 */

#include "render.h"

#include <string.h>

#include "font.h"

/* ---- The surface ---- */

/* Row-major RGB565 framebuffer. 1024*600*2 = 1.2 MB static. */
static uint16_t g_surface[RENDER_HEIGHT * RENDER_WIDTH];

/* Code-Page box-drawing glyph codes (font.c / character-set.md §2). */
#define BOX_S_H   0x80   /* single horizontal */
#define BOX_S_V   0x81   /* single vertical */
#define BOX_S_TL  0x82
#define BOX_S_TR  0x83
#define BOX_S_BL  0x84
#define BOX_S_BR  0x85
#define BOX_D_H   0x8B   /* double horizontal */
#define BOX_D_V   0x8C   /* double vertical */
#define BOX_D_TL  0x8D
#define BOX_D_TR  0x8E
#define BOX_D_BL  0x8F
#define BOX_D_BR  0x90

#define CELL_PX 8

/* ---- Surface lifecycle ---- */

void render_init(void) {
    memset(g_surface, 0, sizeof(g_surface));
}

int render_width(void) {
    return RENDER_WIDTH;
}

int render_height(void) {
    return RENDER_HEIGHT;
}

const uint16_t *render_surface(void) {
    return g_surface;
}

/* ---- Colour helpers ---- */

RenderColors render_colors_normal(PhosphorScheme scheme) {
    RenderColors c;
    c.fg = phosphor_fg_rgb565(scheme);
    c.bg = PHOSPHOR_RGB565_BLACK;
    return c;
}

RenderColors render_colors_inverted(PhosphorScheme scheme) {
    RenderColors c;
    c.fg = PHOSPHOR_RGB565_BLACK;
    c.bg = phosphor_fg_rgb565(scheme);
    return c;
}

/* ---- Primitives ---- */

void render_clear(uint16_t color) {
    /* memset only works for 0x0000 / 0xFFFF byte-uniform values; do a
     * word fill so any colour clears correctly. */
    if (color == 0x0000) {
        memset(g_surface, 0, sizeof(g_surface));
        return;
    }
    size_t n = (size_t)RENDER_WIDTH * (size_t)RENDER_HEIGHT;
    for (size_t i = 0; i < n; i++) {
        g_surface[i] = color;
    }
}

void render_set_pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= RENDER_WIDTH || y < 0 || y >= RENDER_HEIGHT) return;
    g_surface[(size_t)y * RENDER_WIDTH + (size_t)x] = color;
}

uint16_t render_get_pixel(int x, int y) {
    if (x < 0 || x >= RENDER_WIDTH || y < 0 || y >= RENDER_HEIGHT) return 0x0000;
    return g_surface[(size_t)y * RENDER_WIDTH + (size_t)x];
}

void render_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > RENDER_WIDTH) x1 = RENDER_WIDTH;
    int y1 = y + h; if (y1 > RENDER_HEIGHT) y1 = RENDER_HEIGHT;
    for (int py = y0; py < y1; py++) {
        uint16_t *row = &g_surface[(size_t)py * RENDER_WIDTH];
        for (int px = x0; px < x1; px++) {
            row[px] = color;
        }
    }
}

void render_glyph(int x, int y, int scale, uint8_t ch, RenderColors colors) {
    if (scale < 1) scale = 1;
    const uint8_t *glyph = kn86_font + (size_t)ch * 8;
    for (int gy = 0; gy < 8; gy++) {
        uint8_t bits = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            /* MSB-first: bit 7 is the leftmost pixel. */
            uint16_t color = (bits & (0x80 >> gx)) ? colors.fg : colors.bg;
            render_fill_rect(x + gx * scale, y + gy * scale, scale, scale, color);
        }
    }
}

void render_text(int x, int y, int scale, const char *str, RenderColors colors) {
    if (str == NULL) return;
    if (scale < 1) scale = 1;
    int cx = x;
    for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; p++) {
        render_glyph(cx, y, scale, (uint8_t)*p, colors);
        cx += 8 * scale;
    }
}

void render_bitmap(int x, int y, int scale, const uint8_t *bits,
                   int bw, int bh, RenderColors colors, bool opaque) {
    if (bits == NULL || bw <= 0 || bh <= 0) return;
    if (scale < 1) scale = 1;
    int stride = (bw + 7) / 8; /* bytes per row, MSB-first */
    for (int by = 0; by < bh; by++) {
        const uint8_t *row = bits + (size_t)by * stride;
        for (int bx = 0; bx < bw; bx++) {
            int set = row[bx / 8] & (0x80 >> (bx % 8));
            if (set) {
                render_fill_rect(x + bx * scale, y + by * scale, scale, scale,
                                 colors.fg);
            } else if (opaque) {
                render_fill_rect(x + bx * scale, y + by * scale, scale, scale,
                                 colors.bg);
            }
        }
    }
}

/* Draw a glyph at scale 1 anchored to a 1x cell coordinate. */
static void glyph_at_cell(int col, int row, uint8_t ch, RenderColors colors) {
    render_glyph(col * CELL_PX, row * CELL_PX, 1, ch, colors);
}

void render_box(int col, int row, int cw, int ch, const char *title,
                RenderBoxWeight weight, RenderColors colors) {
    if (cw < 2 || ch < 2) return;

    uint8_t g_h, g_v, g_tl, g_tr, g_bl, g_br;
    if (weight == RENDER_BOX_DOUBLE) {
        g_h = BOX_D_H; g_v = BOX_D_V;
        g_tl = BOX_D_TL; g_tr = BOX_D_TR; g_bl = BOX_D_BL; g_br = BOX_D_BR;
    } else {
        g_h = BOX_S_H; g_v = BOX_S_V;
        g_tl = BOX_S_TL; g_tr = BOX_S_TR; g_bl = BOX_S_BL; g_br = BOX_S_BR;
    }

    int right = col + cw - 1;
    int bottom = row + ch - 1;

    /* Corners. */
    glyph_at_cell(col,   row,    g_tl, colors);
    glyph_at_cell(right, row,    g_tr, colors);
    glyph_at_cell(col,   bottom, g_bl, colors);
    glyph_at_cell(right, bottom, g_br, colors);

    /* Horizontal edges (top + bottom). */
    for (int c = col + 1; c < right; c++) {
        glyph_at_cell(c, row,    g_h, colors);
        glyph_at_cell(c, bottom, g_h, colors);
    }
    /* Vertical edges (left + right). */
    for (int r = row + 1; r < bottom; r++) {
        glyph_at_cell(col,   r, g_v, colors);
        glyph_at_cell(right, r, g_v, colors);
    }

    /* Title cutout (ui-design-language.md §6.1): clear the title cells
     * plus a one-cell pad on each side, then stamp the title, so the top
     * border does not strike through the title text. */
    if (title != NULL && title[0] != '\0') {
        int title_len = (int)strlen(title);
        int cut_start = col + 2;              /* conventional offset */
        int cut_width = title_len + 2;        /* one-cell pad each side */
        int cut_end = cut_start + cut_width;  /* exclusive */
        /* Do not run the cutout past the right corner. */
        if (cut_end > right) {
            cut_end = right;
        }
        /* Clear the cutout span on the top border to background. */
        for (int c = cut_start; c < cut_end && c < right; c++) {
            glyph_at_cell(c, row, ' ', colors);
        }
        /* Stamp the title one cell into the cleared span (after the pad),
         * clipping at the right corner. */
        int tcol = cut_start + 1;
        for (int i = 0; i < title_len; i++) {
            int c = tcol + i;
            if (c >= right) break;
            glyph_at_cell(c, row, (uint8_t)title[i], colors);
        }
    }
}

/* ---- Half-block layer (ADR-0036 §Decision item 6) ----
 *
 * The 128x150 pseudo-pixel canvas: each 1x cell (8x8 px) carries 2
 * vertical sub-pixels, so a half-block pixel is 8 px wide x 4 px tall.
 * (hx, hy) addresses the canvas; this is the system-tier convenience
 * over set_pixel that ADR-0036 §Decision-6 specifies. */
void render_half_block_set(int hx, int hy, int on) {
    if (hx < 0 || hx >= 128 || hy < 0 || hy >= 150) return;
    uint16_t color = on ? phosphor_active_fg_rgb565() : PHOSPHOR_RGB565_BLACK;
    int px = hx * CELL_PX;
    int py = (hy / 2) * CELL_PX + (hy & 1) * (CELL_PX / 2);
    render_fill_rect(px, py, CELL_PX, CELL_PX / 2, color);
}

/* ====================================================================
 * SDL3 desktop backend (ADR-0025 / ADR-0036 §Decision item 8).
 * Compiled live only with KN86_RENDER_SDL; harmless stubs otherwise so
 * the renderer (and its tests) link without SDL. The /dev/fb0 device
 * backend is deferred per ADR-0036 §Decision-8 and not implemented here.
 * ==================================================================== */

#ifdef KN86_RENDER_SDL

#include <stdio.h>
#include <SDL3/SDL.h>

static SDL_Window   *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture  *g_texture;

bool render_present_init(void) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        fprintf(stderr, "render: SDL_InitSubSystem(VIDEO) failed: %s\n",
                SDL_GetError());
        return false;
    }
    g_window = SDL_CreateWindow("KN-86 Deckline", RENDER_WIDTH, RENDER_HEIGHT, 0);
    if (g_window == NULL) {
        fprintf(stderr, "render: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (g_renderer == NULL) {
        fprintf(stderr, "render: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(g_renderer, 1);
    /* The surface is native RGB565; let SDL convert on upload. */
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB565,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  RENDER_WIDTH, RENDER_HEIGHT);
    if (g_texture == NULL) {
        fprintf(stderr, "render: SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void render_present(void) {
    if (g_texture == NULL || g_renderer == NULL) return;
    SDL_UpdateTexture(g_texture, NULL, g_surface, RENDER_WIDTH * (int)sizeof(uint16_t));
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

bool render_poll(void) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) return false;
    }
    return true;
}

void render_present_shutdown(void) {
    if (g_texture)  { SDL_DestroyTexture(g_texture);   g_texture = NULL; }
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window)   { SDL_DestroyWindow(g_window);     g_window = NULL; }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

#else /* !KN86_RENDER_SDL — headless stubs (tests, device path TBD) */

bool render_present_init(void)     { return true; }
void render_present(void)          { }
bool render_poll(void)             { return true; }
void render_present_shutdown(void) { }

#endif /* KN86_RENDER_SDL */
