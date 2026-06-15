/*
 * test_ui_frames.c — ui/ frames + chrome + compositing (GWP-510).
 *
 * Golden-buffer tests for the frame primitives (cbox, panel, rule),
 * chrome rows (status-bar, action-bar), headline, status-glyph, and the
 * compositing primitives (scrim, shadow, select-row, modal, popup,
 * palette). Every test runs through the ui_golden.h harness: a System
 * context with render/ + ui/ loaded, a component form eval'd, the render
 * surface asserted glyph-by-glyph.
 */

#include "ui_golden.h"

/* --- harness sanity --------------------------------------------------- */

KN86_TEST(harness_loads_libs_and_calls_component)
{
    kec_State *S = ui_open();
    KN86_ASSERT_EQ(ui_fe_error_seen, 0);
    fe_Context *ctx = kec_fe(S);
    /* A ui/ symbol resolves and a draw lands on the surface. */
    ui_eval(ctx, "(ui/panel 2 2 10 5 nil)");
    /* top-left corner cell carries the single-line corner glyph. */
    KN86_ASSERT(ui_glyph_normal(2, 2, 0x82));
    kec_close(S);
}

/* --- cbox + the §6.1 title cutout ------------------------------------- */

KN86_TEST(cbox_single_draws_corners_and_edges)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/cbox 3 4 12 6 nil 'single)");
    KN86_ASSERT(ui_glyph_normal(3, 4, 0x82));          /* ┌ */
    KN86_ASSERT(ui_glyph_normal(14, 4, 0x83));         /* ┐ */
    KN86_ASSERT(ui_glyph_normal(3, 9, 0x84));          /* └ */
    KN86_ASSERT(ui_glyph_normal(14, 9, 0x85));         /* ┘ */
    KN86_ASSERT(ui_glyph_normal(4, 4, 0x80));          /* ─ top edge */
    KN86_ASSERT(ui_glyph_normal(3, 5, 0x81));          /* │ left edge */
    kec_close(S);
}

KN86_TEST(cbox_double_uses_double_glyphs)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/cbox 2 2 10 5 nil 'double)");
    KN86_ASSERT(ui_glyph_normal(2, 2, 0x8D));          /* ╔ */
    KN86_ASSERT(ui_glyph_normal(11, 2, 0x8E));         /* ╗ */
    kec_close(S);
}

/* The title cutout (§6.1): the border line must NOT strike through the
 * title cells. With a title, the cells under " TITLE " (offset 2 from the
 * left corner) carry the title text, not the ─ border glyph. */
KN86_TEST(cbox_title_cutout_does_not_strike_through)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* render_box draws the title at scale 1 offset 2 cells from the left,
     * with a one-cell pad: span is " HI " starting at col cx+2. */
    ui_eval(ctx, "(ui/cbox 0 0 14 4 \"HI\" 'single)");
    /* col 2 = pad space, col 3 = 'H', col 4 = 'I', col 5 = pad space. */
    KN86_ASSERT(ui_glyph_normal(3, 0, 'H'));
    KN86_ASSERT(ui_glyph_normal(4, 0, 'I'));
    /* The title cells are NOT the horizontal border glyph 0x80. */
    KN86_ASSERT(!ui_glyph_normal(3, 0, 0x80));
    KN86_ASSERT(!ui_glyph_normal(4, 0, 0x80));
    /* The border resumes past the title pad: a later top cell is still ─. */
    KN86_ASSERT(ui_glyph_normal(8, 0, 0x80));
    kec_close(S);
}

/* --- rule (NEW) ------------------------------------------------------- */

KN86_TEST(rule_draws_horizontal_divider)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/rule 5 10 6)");
    for (int c = 5; c < 11; c++) {
        KN86_ASSERT(ui_glyph_normal(c, 10, 0x80));     /* ─ */
    }
    /* One past the run is blank. */
    KN86_ASSERT_EQ(ui_cell_lit_count(11, 10), 0);
    kec_close(S);
}

/* --- status-bar / action-bar: inverted chrome ------------------------- */

KN86_TEST(status_bar_is_inverted_chrome)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/status-bar \"KZN-7A2B\" \"4210\" \"MISSIONS\")");
    /* Scale-2 band, rows 0..15: an inverted field — black glyphs on an
     * ember band. Past the text the band is the pure ember field; sample
     * far right (x=1000) where no glyph ink lands. */
    KN86_ASSERT_EQ(render_get_pixel(1000, 0), UI_EMBER);   /* field lit */
    KN86_ASSERT_EQ(render_get_pixel(1000, 15), UI_EMBER);  /* band is 16 tall */
    KN86_ASSERT_EQ(render_get_pixel(1000, 16), UI_BLACK);  /* just below band */
    /* The text is the inverted pair: black ink exists in the band. */
    int black_in_band = 0;
    for (int x = 0; x < 64 && !black_in_band; x++)
        for (int y = 0; y < 16; y++)
            if (render_get_pixel(x, y) == UI_BLACK) { black_in_band = 1; break; }
    KN86_ASSERT(black_in_band);
    kec_close(S);
}

KN86_TEST(action_bar_sits_on_bottom_band)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/action-bar '((\"EVAL\" . \"accept\") (\"BACK\" . \"exit\")))");
    /* Bottom 16 px band (rows 584..599) is the lit field. Sample far right
     * (x=1000) past the text, where the band is the pure ember field. */
    KN86_ASSERT_EQ(render_get_pixel(1000, 584), UI_EMBER);
    KN86_ASSERT_EQ(render_get_pixel(1000, 599), UI_EMBER);
    /* Nothing painted above the band (row 583 untouched). */
    KN86_ASSERT_EQ(render_get_pixel(0, 583), UI_BLACK);
    kec_close(S);
}

/* --- headline: big-glyph scale -------------------------------------- */

KN86_TEST(headline_scales_glyph)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* 'A' headline at cell (1,1) scale 4 -> pixel (8,8), 32x32 cell. */
    ui_eval(ctx, "(ui/headline 1 1 \"A\" 4)");
    /* 'A' row 0 byte = 0x18 -> bits 3,4 lit. At scale 4 those occupy
     * pixel columns 12..19 of the glyph, rows 8..11. Assert a lit pixel
     * in that scaled block and that the glyph spans >8 px (it's scaled). */
    int lit = 0;
    for (int y = 8; y < 8 + 32 && !lit; y++)
        for (int x = 8; x < 8 + 32; x++)
            if (render_get_pixel(x, y) == UI_EMBER) { lit = 1; break; }
    KN86_ASSERT(lit);
    /* Scaled: a lit pixel exists beyond the 8px (scale-1) cell width. */
    int wide = 0;
    for (int y = 8; y < 40 && !wide; y++)
        for (int x = 20; x < 40; x++)
            if (render_get_pixel(x, y) == UI_EMBER) { wide = 1; break; }
    KN86_ASSERT(wide);
    kec_close(S);
}

/* --- status-glyph: §4 identity channel ------------------------------ */

KN86_TEST(status_glyph_maps_state_to_icon)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/status-glyph 1 1 'running)");
    KN86_ASSERT(ui_glyph_normal(1, 1, 0x05));          /* ● */
    ui_eval(ctx, "(ui/status-glyph 2 1 'stopped)");
    KN86_ASSERT(ui_glyph_normal(2, 1, 0x06));          /* ○ */
    ui_eval(ctx, "(ui/status-glyph 3 1 'error)");
    KN86_ASSERT(ui_glyph_normal(3, 1, 0xF6));          /* ✖ */
    kec_close(S);
}

KN86_TEST(status_glyph_starting_frame_cycles)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* even frame -> ◐ (0xF5), odd frame -> ○ (0x06). */
    ui_eval(ctx, "(ui/status-glyph-anim 1 1 'starting 0)");
    KN86_ASSERT(ui_glyph_normal(1, 1, 0xF5));
    render_init();
    ui_eval(ctx, "(ui/status-glyph-anim 1 1 'starting 1)");
    KN86_ASSERT(ui_glyph_normal(1, 1, 0x06));
    kec_close(S);
}

/* --- select-row: the §4 selection primitive ------------------------- */

KN86_TEST(select_row_inverts_a_span)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/select-row 4 6 5)");
    /* 5 cells at row 6 (cols 4..8) are a solid lit fill. */
    for (int c = 4; c < 9; c++)
        KN86_ASSERT(ui_cell_solid(c, 6, UI_EMBER));
    /* One past the span is black. */
    KN86_ASSERT_EQ(ui_cell_lit_count(9, 6), 0);
    kec_close(S);
}

/* --- scrim: dither depth cue ---------------------------------------- */

KN86_TEST(scrim_stamps_dither_cells)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/scrim 2 2 4 3)");
    /* Each cell in the region is the med-shade dither glyph (0xA6). */
    KN86_ASSERT(ui_glyph_normal(2, 2, 0xA6));
    KN86_ASSERT(ui_glyph_normal(5, 4, 0xA6));
    /* A dither cell is half-lit (the 0xAA/0x55 checkerboard = 32 px). */
    KN86_ASSERT_EQ(ui_cell_lit_count(3, 3), 32);
    kec_close(S);
}

/* --- shadow: drop-shadow depth cue ---------------------------------- */

KN86_TEST(shadow_is_black_offset_strips)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* Lay a lit field first so the black shadow is visible against it. */
    ui_eval(ctx, "(render/fill-rect 0 0 1024 600 1)");
    ui_eval(ctx, "(ui/shadow 2 2 6 4)");
    /* Right strip: cell column (2+6)=8, rows (2+1)..(2+4) -> a black cell. */
    KN86_ASSERT(ui_cell_solid(8, 3, UI_BLACK));
    /* Bottom strip: cell row (2+4)=6, cols (2+1)..(2+6) -> a black cell. */
    KN86_ASSERT(ui_cell_solid(4, 6, UI_BLACK));
    /* The frame interior corner (cell 4,3) is untouched (still lit). */
    KN86_ASSERT(ui_cell_solid(4, 3, UI_EMBER));
    kec_close(S);
}

/* --- modal: shadow + scrim'd compose + double border + black fill ---- */

KN86_TEST(modal_lays_black_fill_double_border_and_shadow)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* Scrim a background so the shadow reads (pre-condition §8.3). */
    ui_eval(ctx, "(ui/scrim 0 0 40 20)");
    ui_eval(ctx, "(ui/modal 5 5 20 8 \"DIALOG\" nil)");
    /* Double-line corners. */
    KN86_ASSERT(ui_glyph_normal(5, 5, 0x8D));          /* ╔ */
    KN86_ASSERT(ui_glyph_normal(24, 5, 0x8E));         /* ╗ */
    /* Interior is cleared to black (a cell well inside the frame). */
    KN86_ASSERT(ui_cell_solid(10, 7, UI_BLACK));
    /* Drop shadow: black cell at the +1/+1 right strip (cell col 25). */
    KN86_ASSERT(ui_cell_solid(25, 8, UI_BLACK));
    /* Title cutout intact: render_box stamps the title at col+3 (offset 2
     * corner + 1 pad). Modal at cx=5 -> 'D' of DIALOG at col 8. */
    KN86_ASSERT(ui_glyph_normal(8, 5, 'D'));
    kec_close(S);
}

KN86_TEST(modal_body_fn_draws_contents)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/scrim 0 0 40 20)");
    ui_eval(ctx,
        "(ui/modal 4 4 20 6 nil (fn () (render/text 48 40 1 1 \"OK\")))");
    /* 'OK' painted by the body-fn at pixel (48,40) = cell (6,5). */
    KN86_ASSERT(ui_glyph_normal(6, 5, 'O'));
    KN86_ASSERT(ui_glyph_normal(7, 5, 'K'));
    kec_close(S);
}

/* --- popup: anchored modal + selected first row --------------------- */

KN86_TEST(popup_renders_items_first_selected)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/scrim 0 0 40 20)");
    ui_eval(ctx,
        "(ui/popup '(3 . 3) '(\"car\" \"cdr\" \"cons\") nil)");
    /* The popup frames at anchor (3,3), double-line corner. */
    KN86_ASSERT(ui_glyph_normal(3, 3, 0x8D));
    /* First item row is inverted: 'c' of "car" black-on-phosphor at the
     * interior column (anchor col + 2). */
    KN86_ASSERT(ui_glyph_inverted(5, 4, 'c'));
    kec_close(S);
}

/* --- palette: centered, prompt + separator + selected + footer ------- */

KN86_TEST(palette_centers_and_selects_first)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/scrim 0 0 128 75)");
    ui_eval(ctx, "(ui/palette '(\"run\" \"build\" \"flash\") \"r\")");
    /* cw=64, ch=3+6=9, cx=(128-64)/2=32, cy=(75-9)/2=33. */
    KN86_ASSERT(ui_glyph_normal(32, 33, 0x8D));        /* ╔ top-left */
    /* First item (row cy+4=37) is the selected/inverted row carrying a ►
     * lead glyph in the inverted pair at interior col cx+2=34. */
    KN86_ASSERT(ui_glyph_inverted(34, 37, 0x0C));      /* ► batch glyph */
    kec_close(S);
}

int main(void)
{
    KN86_RUN_TEST(harness_loads_libs_and_calls_component);
    KN86_RUN_TEST(cbox_single_draws_corners_and_edges);
    KN86_RUN_TEST(cbox_double_uses_double_glyphs);
    KN86_RUN_TEST(cbox_title_cutout_does_not_strike_through);
    KN86_RUN_TEST(rule_draws_horizontal_divider);
    KN86_RUN_TEST(status_bar_is_inverted_chrome);
    KN86_RUN_TEST(action_bar_sits_on_bottom_band);
    KN86_RUN_TEST(headline_scales_glyph);
    KN86_RUN_TEST(status_glyph_maps_state_to_icon);
    KN86_RUN_TEST(status_glyph_starting_frame_cycles);
    KN86_RUN_TEST(select_row_inverts_a_span);
    KN86_RUN_TEST(scrim_stamps_dither_cells);
    KN86_RUN_TEST(shadow_is_black_offset_strips);
    KN86_RUN_TEST(modal_lays_black_fill_double_border_and_shadow);
    KN86_RUN_TEST(modal_body_fn_draws_contents);
    KN86_RUN_TEST(popup_renders_items_first_selected);
    KN86_RUN_TEST(palette_centers_and_selects_first);
    return KN86_REPORT();
}
