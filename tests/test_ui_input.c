/*
 * test_ui_input.c — ui/ nav + input + transition (GWP-512).
 *
 * Golden-buffer tests for tab-bar, field, form, slider, dial, cycler,
 * toast, and the reveal/unreveal transition wrappers. Asserts the §4
 * four-channel rule: the focused tab / field / cursor cell / current enum
 * option is the inverted pair; the continuous controls fill at sub-cell
 * precision. The reveal wrapper test pins the documented reachability
 * boundary — `reveal` is unbound in a System context, so ui/reveal raises.
 */

#include "ui_golden.h"

/* --- tab-bar ---------------------------------------------------------- */

KN86_TEST(tab_bar_numbers_and_selects)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* tabs STATUS / CIPHER, index 1 selected. Cell 0 of tab 0 = '1'. */
    ui_eval(ctx, "(ui/tab-bar 0 0 '(\"STATUS\" \"CIPHER\") 1)");
    KN86_ASSERT(ui_glyph_normal(0, 0, '1'));           /* 1:STATUS */
    KN86_ASSERT(ui_glyph_normal(1, 0, ':'));
    /* tab 0 "1:STATUS" is 8 cells, +1 gap -> tab 1 starts at col 9.
     * tab 1 "2:CIPHER" is selected -> inverted '2' at col 9. */
    KN86_ASSERT(ui_glyph_inverted(9, 0, '2'));
    kec_close(S);
}

/* --- field (NEW): single-line editable + caret ---------------------- */

KN86_TEST(field_renders_text_and_caret)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* "abc" with caret at column 1 -> 'b' is the caret cell (inverted). */
    ui_eval(ctx, "(ui/field 2 3 10 \"abc\" 1)");
    KN86_ASSERT(ui_glyph_normal(2, 3, 'a'));           /* 'a' normal */
    KN86_ASSERT(ui_glyph_inverted(3, 3, 'b'));         /* caret cell */
    KN86_ASSERT(ui_glyph_normal(4, 3, 'c'));           /* 'c' normal */
    kec_close(S);
}

KN86_TEST(field_caret_past_text_is_solid_block)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* "hi" with caret at column 2 (past end) -> a solid lit caret cell. */
    ui_eval(ctx, "(ui/field 0 0 8 \"hi\" 2)");
    KN86_ASSERT(ui_cell_solid(2, 0, UI_AMBER));        /* caret over space */
    kec_close(S);
}

KN86_TEST(field_no_caret_is_readonly_echo)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/field 0 0 8 \"hi\" nil)");
    /* no inverted cell anywhere — read-only echo. */
    KN86_ASSERT(!ui_cell_solid(0, 0, UI_AMBER));
    KN86_ASSERT(!ui_cell_solid(2, 0, UI_AMBER));
    KN86_ASSERT(ui_glyph_normal(0, 0, 'h'));
    kec_close(S);
}

/* --- form: focused field inverted ----------------------------------- */

KN86_TEST(form_focused_field_has_caret)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* two fields, ("NAME"."abe") ("BAL"."10"); focus index 0. label width
     * 4 -> value column at cx + 4 + 2 = 6. */
    ui_eval(ctx,
        "(ui/form 0 0 20 '((\"NAME\" . \"abe\") (\"BAL\" . \"10\")) 0)");
    /* label 'N' at col 0. */
    KN86_ASSERT(ui_glyph_normal(0, 0, 'N'));
    /* focused field 0 value "abe" at col 6, caret at end (col 6+3=9) ->
     * a solid lit caret cell. */
    KN86_ASSERT(ui_glyph_normal(6, 0, 'a'));
    KN86_ASSERT(ui_cell_solid(9, 0, UI_AMBER));        /* caret past "abe" */
    /* unfocused field 1 value "10" at col 6 row 1, NO caret. */
    KN86_ASSERT(ui_glyph_normal(6, 1, '1'));
    KN86_ASSERT(!ui_cell_solid(8, 1, UI_AMBER));
    kec_close(S);
}

/* --- slider: cursor at value position ------------------------------- */

KN86_TEST(slider_cursor_at_value)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* cw=9, value 5 over [0,10] -> frac 0.5 -> pos round(0.5*8)=4. The
     * cursor cell (col 0+4) is inverted; the rest is the track line. */
    ui_eval(ctx, "(ui/slider 0 3 9 5 0 10)");
    KN86_ASSERT(ui_cell_solid(4, 3, UI_AMBER));        /* cursor */
    /* a non-cursor track cell is the horizontal line glyph (normal). */
    KN86_ASSERT(ui_glyph_normal(0, 3, 0x80));
    kec_close(S);
}

KN86_TEST(slider_clamps_to_ends)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* value at max -> cursor on the last cell (col cw-1 = 4). */
    ui_eval(ctx, "(ui/slider 0 0 5 10 0 10)");
    KN86_ASSERT(ui_cell_solid(4, 0, UI_AMBER));
    kec_close(S);
}

/* --- dial: sub-cell fill + readout ---------------------------------- */

KN86_TEST(dial_fills_and_reads_out)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* 6x6 box, value 50 over [0,100] -> 50% fill. Interior height 4 cells
     * = 8 half-cells; 50% -> 4 half-cells = 16 px, bottom-anchored. */
    ui_eval(ctx, "(ui/dial 0 0 6 6 50 0 100)");
    /* single-line frame corner. */
    KN86_ASSERT(ui_glyph_normal(0, 0, 0x82));
    /* the box bottom-interior pixel (just above the bottom border) is lit
     * from the fill; the interior bottom border is at cell row 5 (px 40);
     * fill occupies px rows (40-16)=24..39 inside cols 8..39. */
    KN86_ASSERT_EQ(render_get_pixel(12, 38), UI_AMBER);   /* in the fill */
    KN86_ASSERT_EQ(render_get_pixel(12, 16), UI_BLACK);   /* above the fill */
    /* numeric readout '5' of "50" at the top interior cell (1,1),
     * inverted pair over a possibly-empty top region (so black ink). */
    KN86_ASSERT(ui_glyph_inverted(1, 1, '5'));
    kec_close(S);
}

/* --- cycler: current option inverted -------------------------------- */

KN86_TEST(cycler_marks_current_option)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* MODE: AMBER > WHITE > GREEN, current "WHITE". label width 4 ->
     * options start at col 6. "AMBER"(5) + ">"(1) -> WHITE starts col 12. */
    ui_eval(ctx,
        "(ui/cycler 0 0 \"MODE\" \"WHITE\" '(\"AMBER\" \"WHITE\" \"GREEN\"))");
    /* "AMBER" is the un-selected option: normal 'A' at col 6. */
    KN86_ASSERT(ui_glyph_normal(6, 0, 'A'));
    /* current "WHITE": inverted 'W' at col 12. */
    KN86_ASSERT(ui_glyph_inverted(12, 0, 'W'));
    kec_close(S);
}

/* --- toast: transient inverted band --------------------------------- */

KN86_TEST(toast_is_centered_inverted_band)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* "saved" (5 chars) -> band width 7, centered: cx = (128-7)/2 = 60,
     * at row 72. The band is inverted (lit) with black text. */
    ui_eval(ctx, "(ui/toast \"saved\" 'ok)");
    KN86_ASSERT(ui_cell_solid(60, 72, UI_AMBER));      /* left pad lit */
    KN86_ASSERT(ui_glyph_inverted(61, 72, 's'));       /* text black-on-lit */
    /* the toast captures no focus / draws no border above its row. */
    KN86_ASSERT_EQ(ui_cell_lit_count(60, 71), 0);
    kec_close(S);
}

/* --- reveal / unreveal: defined, but unbound underneath (documented) - */

KN86_TEST(reveal_wrappers_are_defined)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* The wrappers load without raising (defining a fn whose body names an
     * unbound symbol is fine — the symbol resolves only on call). They are
     * bound symbols of type fn. */
    KN86_ASSERT_EQ(ui_fe_error_seen, 0);
    fe_Object *r = ui_eval(ctx, "(fn? ui/reveal)");
    KN86_ASSERT(!fe_isnil(ctx, r));
    fe_Object *u = ui_eval(ctx, "(fn? ui/unreveal)");
    KN86_ASSERT(!fe_isnil(ctx, u));
    kec_close(S);
}

/* Calling ui/reveal raises: the underlying (reveal …) primitive is bound
 * by the cart/runtime FFI bridge, NOT the System render tier — the same
 * capability boundary the render-tier split enforces. This test pins that
 * documented dependency. */
KN86_TEST(reveal_call_raises_unbound_in_system_context)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    fe_handlers(ctx)->error = ui_record_fe_error;

    ui_fe_error_seen = 0;
    if (setjmp(ui_fe_error_jmp) == 0) {
        ui_eval(ctx, "(ui/reveal ':char-flicker)");
        KN86_ASSERT(0 && "ui/reveal should raise — reveal is unbound here");
    } else {
        KN86_ASSERT_EQ(ui_fe_error_seen, 1);
    }
    kec_close(S);
}

int main(void)
{
    KN86_RUN_TEST(tab_bar_numbers_and_selects);
    KN86_RUN_TEST(field_renders_text_and_caret);
    KN86_RUN_TEST(field_caret_past_text_is_solid_block);
    KN86_RUN_TEST(field_no_caret_is_readonly_echo);
    KN86_RUN_TEST(form_focused_field_has_caret);
    KN86_RUN_TEST(slider_cursor_at_value);
    KN86_RUN_TEST(slider_clamps_to_ends);
    KN86_RUN_TEST(dial_fills_and_reads_out);
    KN86_RUN_TEST(cycler_marks_current_option);
    KN86_RUN_TEST(toast_is_centered_inverted_band);
    KN86_RUN_TEST(reveal_wrappers_are_defined);
    KN86_RUN_TEST(reveal_call_raises_unbound_in_system_context);
    return KN86_REPORT();
}
