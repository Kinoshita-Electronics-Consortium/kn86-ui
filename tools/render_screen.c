/*
 * render_screen.c — render a KN-86 screen DSL tree to an image.
 *
 * Opens a System-tier KEC context (the same one the golden-buffer tests
 * use), loads render/ + ui/ + ui/screen.lsp, evals a screen file (which
 * calls (ui/draw '(screen ...))), then dumps the real RGB565 render surface
 * to a binary PPM (P6). The PPM is the exact framebuffer the device panel
 * would receive — convert to PNG downstream for viewing.
 *
 *   render_screen <screen.lsp> <out.ppm>
 *
 * This is the DSL preview path: same C renderer + font.c the panel uses, no
 * SDL, no mock. See docs/plans/2026-06-16-ui-render-engine-design.md.
 */
#include "ui_golden.h"   /* ui_open / ui_eval / ui_slurp + render.h, kec, fe */

#include <stdio.h>

static char g_filebuf[128 * 1024];

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: render_screen <screen.lsp> <out.ppm>\n");
        return 2;
    }

    kec_State *S = ui_open();           /* loads render/ + ui/ kit */
    if (S == NULL) {
        fprintf(stderr, "render_screen: ui_open failed\n");
        return 1;
    }
    fe_Context *ctx = kec_fe(S);

    /* load the engine (not in the default kit load list yet) */
    char path[512];
    snprintf(path, sizeof(path), "%s/ui/screen.lsp", UI_LIB_DIR);
    if (ui_slurp(path, g_filebuf, sizeof(g_filebuf)) <= 0) {
        fprintf(stderr, "render_screen: cannot read %s\n", path);
        return 1;
    }
    ui_eval(ctx, g_filebuf);

    /* load + eval the screen file (it calls ui/draw) */
    if (ui_slurp(argv[1], g_filebuf, sizeof(g_filebuf)) <= 0) {
        fprintf(stderr, "render_screen: cannot read %s\n", argv[1]);
        return 1;
    }
    ui_eval(ctx, g_filebuf);

    /* dump the surface to PPM (P6): RGB565 -> RGB888 */
    const uint16_t *surf = render_surface();
    int w = render_width(), h = render_height();
    FILE *f = fopen(argv[2], "wb");
    if (f == NULL) {
        fprintf(stderr, "render_screen: cannot write %s\n", argv[2]);
        return 1;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint16_t v = surf[i];
        unsigned char r = (unsigned char)(((v >> 11) & 0x1F) << 3);
        unsigned char g = (unsigned char)(((v >> 5) & 0x3F) << 2);
        unsigned char b = (unsigned char)((v & 0x1F) << 3);
        fputc(r, f); fputc(g, f); fputc(b, f);
    }
    fclose(f);
    fprintf(stderr, "render_screen: wrote %s (%dx%d)\n", argv[2], w, h);
    return 0;
}
