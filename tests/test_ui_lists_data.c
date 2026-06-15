/*
 * test_ui_lists_data.c — ui/ lists + trees + tables + data (GWP-511).
 *
 * Golden-buffer tests for the data-navigation widgets (list, tree, table)
 * and the magnitude primitives (sparkline, fine-bar). Asserts the §4
 * four-channel rule: selected rows are the inverted pair, leading-glyph /
 * disclosure / typed-column glyphs are the identity channel, and the bar
 * ramps map values to the right vbar / hbar glyph codes.
 */

#include "ui_golden.h"

/* --- list ------------------------------------------------------------- */

KN86_TEST(list_renders_rows_with_leading_glyph)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* leading-glyph-fn returns ● (5) for index 0, space otherwise. */
    ui_eval(ctx,
        "(ui/list 2 2 16 4 '(\"alpha\" \"beta\") nil "
        "  (fn (i row) (if (is i 0) 5 32)))");
    /* row 0: lead glyph ● at col 2, text 'a' at col 4 (cx+2). */
    KN86_ASSERT(ui_glyph_normal(2, 2, 0x05));
    KN86_ASSERT(ui_glyph_normal(4, 2, 'a'));
    /* row 1: space lead, 'b' at col 4 row 3. */
    KN86_ASSERT(ui_glyph_normal(4, 3, 'b'));
    kec_close(S);
}

KN86_TEST(list_selected_row_is_inverted_pair)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx,
        "(ui/list 2 2 16 4 '(\"alpha\" \"beta\") 1 nil)");
    /* row 1 (index 1) is selected: the span (cols 2..17) is filled lit,
     * and the text 'b' is black-on-phosphor (inverted pair). */
    KN86_ASSERT(ui_cell_solid(2, 3, UI_EMBER));        /* lead column lit */
    KN86_ASSERT(ui_glyph_inverted(4, 3, 'b'));         /* black-on-ember */
    /* row 0 is normal: 'a' phosphor-on-black. */
    KN86_ASSERT(ui_glyph_normal(4, 2, 'a'));
    kec_close(S);
}

/* Scrollback / log usage: a list with selected=nil is a log pane — no
 * inverted row anywhere. */
KN86_TEST(list_no_selection_is_a_log_pane)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/list 0 0 32 3 '(\"> a\" \"> b\") nil nil)");
    /* No fully-lit (inverted) cell exists in the rendered rows — every
     * row is phosphor-on-black. Sample the lead column of each row. */
    KN86_ASSERT(!ui_cell_solid(0, 0, UI_EMBER));
    KN86_ASSERT(!ui_cell_solid(0, 1, UI_EMBER));
    kec_close(S);
}

KN86_TEST(list_windows_to_ch_rows)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* 4 rows of data, ch=2 -> only rows 0,1 render; row 2 stays blank. */
    ui_eval(ctx, "(ui/list 0 0 8 2 '(\"a\" \"b\" \"c\" \"d\") nil nil)");
    KN86_ASSERT(ui_glyph_normal(2, 0, 'a'));
    KN86_ASSERT(ui_glyph_normal(2, 1, 'b'));
    KN86_ASSERT_EQ(ui_cell_lit_count(2, 2), 0);        /* 'c' not drawn */
    kec_close(S);
}

/* --- tree ------------------------------------------------------------- */

KN86_TEST(tree_collapsed_shows_arrow_and_hides_children)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* root has a child; expanded? is always-false -> collapsed. */
    ui_eval(ctx,
        "(ui/tree 0 0 20 6 '(\"root\" (\"child\")) (fn (n) nil) nil)");
    /* root disclosure glyph is ► (collapsed, 0x0C) at col 0. */
    KN86_ASSERT(ui_glyph_normal(0, 0, 0x0C));
    /* label 'r' of root at col 2. */
    KN86_ASSERT(ui_glyph_normal(2, 0, 'r'));
    /* child is hidden: row 1 is blank. */
    KN86_ASSERT_EQ(ui_cell_lit_count(2, 1), 0);
    kec_close(S);
}

KN86_TEST(tree_expanded_shows_children_indented)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* expanded? always-true -> root open, child shown indented. */
    ui_eval(ctx,
        "(ui/tree 0 0 20 6 '(\"root\" (\"kid\")) (fn (n) 1) nil)");
    /* root disclosure ▼ (expanded, 0x0A) at col 0. */
    KN86_ASSERT(ui_glyph_normal(0, 0, 0x0A));
    /* child at depth 1 -> indent 2 cells; leaf disclosure = space; label
     * 'k' at col (2 + 2). */
    KN86_ASSERT(ui_glyph_normal(4, 1, 'k'));
    kec_close(S);
}

KN86_TEST(tree_selected_node_inverted)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* select the root node itself. Bind the structure once so the select
     * arg is `is`-identical to the root passed to ui/tree. */
    ui_eval(ctx,
        "(let t '(\"root\" (\"kid\")))"
        "(ui/tree 0 0 20 6 t (fn (n) nil) t)");
    /* root row inverted: 'r' label black-on-phosphor at col 2. */
    KN86_ASSERT(ui_glyph_inverted(2, 0, 'r'));
    kec_close(S);
}

/* --- table ------------------------------------------------------------ */

KN86_TEST(table_header_is_inverted_with_typed_glyphs)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* two columns: ("NAME".6) ("BAL".6); type glyphs @ (64) and $ (36). */
    ui_eval(ctx,
        "(ui/table 0 0 24 4 "
        "  '((\"NAME\" . 6) (\"BAL\" . 6)) "
        "  '((\"abe\" \"10\") (\"cy\" \"20\")) "
        "  1 (list 64 36))");
    /* header row 0 is inverted: type glyph '@' (0x40) black-on-phosphor at
     * col 0, label 'N' black-on-phosphor at col 1. */
    KN86_ASSERT(ui_glyph_inverted(0, 0, '@'));
    KN86_ASSERT(ui_glyph_inverted(1, 0, 'N'));
    /* second column type glyph '$' (0x24) at its column start (col 7). */
    KN86_ASSERT(ui_glyph_inverted(7, 0, '$'));
    /* body row 1: 'a' of "abe" normal pair at col 0. */
    KN86_ASSERT(ui_glyph_normal(0, 1, 'a'));
    kec_close(S);
}

/* --- sparkline: §4 magnitude via vbars 0x96..0x9D -------------------- */

KN86_TEST(sparkline_maps_values_to_vbar_glyphs)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* data (0 4 8) against max 8 -> steps 0, 4, 8 -> glyphs space(0x20),
     * 0x96+3=0x99 (4/8), 0x9D (8/8 full). */
    ui_eval(ctx, "(ui/sparkline 0 5 8 '(0 4 8))");
    KN86_ASSERT(ui_glyph_normal(0, 5, 0x20));          /* empty -> space */
    KN86_ASSERT(ui_glyph_normal(1, 5, 0x99));          /* 4/8 -> ▄ */
    KN86_ASSERT(ui_glyph_normal(2, 5, 0x9D));          /* 8/8 -> █ */
    kec_close(S);
}

KN86_TEST(sparkline_windows_to_newest)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* 4 samples, cw=2 -> newest 2 (the tail: 8 then 8). */
    ui_eval(ctx, "(ui/sparkline 0 5 2 '(1 1 8 8))");
    KN86_ASSERT(ui_glyph_normal(0, 5, 0x9D));          /* 8/8 */
    KN86_ASSERT(ui_glyph_normal(1, 5, 0x9D));          /* 8/8 */
    /* the third cell is not drawn. */
    KN86_ASSERT_EQ(ui_cell_lit_count(2, 5), 0);
    kec_close(S);
}

/* --- fine-bar: §4 magnitude via hbars 0xA8..0xAE + 0xA2 -------------- */

KN86_TEST(fine_bar_full_blocks_plus_partial)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* cw=4 cells = 32 eighths. value 20 / max 32 -> 20 eighths = 2 full
     * cells (16 eighths) + 4-eighths partial in cell 2; cell 3 empty. */
    ui_eval(ctx, "(ui/fine-bar 0 5 4 20 32)");
    KN86_ASSERT(ui_glyph_normal(0, 5, 0xA2));          /* full block */
    KN86_ASSERT(ui_glyph_normal(1, 5, 0xA2));          /* full block */
    KN86_ASSERT(ui_glyph_normal(2, 5, 0xAB));          /* 4/8 left-fill */
    KN86_ASSERT(ui_glyph_normal(3, 5, 0x20));          /* empty */
    kec_close(S);
}

KN86_TEST(fine_bar_full_value_all_blocks)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    /* value == max -> every cell a full block. */
    ui_eval(ctx, "(ui/fine-bar 0 5 3 10 10)");
    KN86_ASSERT(ui_glyph_normal(0, 5, 0xA2));
    KN86_ASSERT(ui_glyph_normal(1, 5, 0xA2));
    KN86_ASSERT(ui_glyph_normal(2, 5, 0xA2));
    kec_close(S);
}

KN86_TEST(fine_bar_zero_is_empty)
{
    kec_State *S = ui_open();
    fe_Context *ctx = kec_fe(S);
    ui_eval(ctx, "(ui/fine-bar 0 5 3 0 10)");
    KN86_ASSERT_EQ(ui_cell_lit_count(0, 5), 0);
    KN86_ASSERT_EQ(ui_cell_lit_count(2, 5), 0);
    kec_close(S);
}

int main(void)
{
    KN86_RUN_TEST(list_renders_rows_with_leading_glyph);
    KN86_RUN_TEST(list_selected_row_is_inverted_pair);
    KN86_RUN_TEST(list_no_selection_is_a_log_pane);
    KN86_RUN_TEST(list_windows_to_ch_rows);
    KN86_RUN_TEST(tree_collapsed_shows_arrow_and_hides_children);
    KN86_RUN_TEST(tree_expanded_shows_children_indented);
    KN86_RUN_TEST(tree_selected_node_inverted);
    KN86_RUN_TEST(table_header_is_inverted_with_typed_glyphs);
    KN86_RUN_TEST(sparkline_maps_values_to_vbar_glyphs);
    KN86_RUN_TEST(sparkline_windows_to_newest);
    KN86_RUN_TEST(fine_bar_full_blocks_plus_partial);
    KN86_RUN_TEST(fine_bar_full_value_all_blocks);
    KN86_RUN_TEST(fine_bar_zero_is_empty);
    return KN86_REPORT();
}
