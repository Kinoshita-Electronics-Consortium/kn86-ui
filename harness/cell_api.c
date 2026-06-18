/*
 * cell_api.c — Backbuffer + chrome-enforcement implementation of the
 * constrained KN-86 cell API (ADR-0027 / GWP-382/S3).
 *
 * Design:
 *   - One static cell grid (128 cols x 75 rows) and one static
 *     half-block sub-pixel canvas (128 cols x 146 rows) live here.
 *     No malloc; pre-allocated as file-scope arrays per CLAUDE.md
 *     "no malloc, pre-allocate or arena."
 *   - The cart-facing draw primitives (cell_set, cell_print,
 *     half_block_*) write into the backbuffer. Chrome rows (0, 74)
 *     are gated; gated drops increment cart_chrome_violation_count.
 *   - present() / yield() are intentionally no-ops in this TU.
 *     The kn86-nosh-tb runtime will own the present-to-termbox
 *     bridge (Wave-2 work). Keeping the bridge OUT of this TU is
 *     deliberate: the spike's whole point is that cartridges only
 *     see this surface. Linking termbox here would let a future
 *     change accidentally widen the cart-visible boundary.
 *   - Inspection helpers (cell_peek_*, half_block_peek) exist for
 *     the test harness and runtime instrumentation. The cart FFI
 *     binder will bind only the 10 builtins listed in cell_api.h's
 *     header comment; the binder must not expose the inspection
 *     helpers, the unchecked chrome writer, or the reset entry.
 */

#include "cell_api.h"

#include <string.h>

/* --- Backbuffer ------------------------------------------------------ */

typedef struct {
    uint32_t     ch;
    cell_color_t fg;
    cell_color_t bg;
} CellSlot;

static CellSlot g_cells[CELL_API_ROWS_TOTAL][CELL_API_COLS];

/* Half-block canvas: one byte per sub-pixel (0 = off, 1 = on).
 * 128 x 146 = 18688 bytes. Tiny. */
static uint8_t g_halfblock[CELL_API_HB_ROWS][CELL_API_HB_COLS];

static unsigned g_chrome_violation_count;

/* --- Internal helpers ------------------------------------------------ */

static int
row_is_chrome_or_out_of_range(int row)
{
    if (row < 0 || row >= CELL_API_ROWS_TOTAL) {
        return 1;
    }
    if (row == CELL_API_CHROME_ROW_TOP || row == CELL_API_CHROME_ROW_BOTTOM) {
        return 1;
    }
    return 0;
}

static int
col_in_range(int col)
{
    return col >= 0 && col < CELL_API_COLS;
}

/* --- Cell grid primitives ------------------------------------------- */

void
cell_set(int col, int row, uint32_t ch, cell_color_t fg, cell_color_t bg)
{
    if (row_is_chrome_or_out_of_range(row)) {
        g_chrome_violation_count++;
        return;
    }
    if (!col_in_range(col)) {
        /* Out-of-range column: silent clip, no violation. */
        return;
    }
    g_cells[row][col].ch = ch;
    g_cells[row][col].fg = fg;
    g_cells[row][col].bg = bg;
}

void
cell_print(int col, int row, cell_color_t fg, cell_color_t bg, const char *str)
{
    if (row_is_chrome_or_out_of_range(row)) {
        g_chrome_violation_count++;
        return;
    }
    if (str == NULL) {
        return;
    }

    int x = col;
    for (const char *p = str; *p != '\0'; p++) {
        if (x >= CELL_API_COLS) {
            break; /* Right-edge clip; no wrap onto the next row. */
        }
        if (x >= 0) {
            g_cells[row][x].ch = (uint32_t)(unsigned char)*p;
            g_cells[row][x].fg = fg;
            g_cells[row][x].bg = bg;
        }
        x++;
    }
}

void
cell_clear_cart_region(void)
{
    /* Rows 1..73 inclusive; chrome rows untouched. */
    for (int r = 1; r <= CELL_API_ROWS_USABLE; r++) {
        memset(g_cells[r], 0, sizeof(g_cells[r]));
    }
}

int
cell_cols(void)
{
    return CELL_API_COLS;
}

int
cell_rows_usable(void)
{
    return CELL_API_ROWS_USABLE;
}

/* --- Half-block canvas primitives ----------------------------------- */

void
half_block_set(int x, int y, int on)
{
    if (x < 0 || x >= CELL_API_HB_COLS) {
        return;
    }
    if (y < 0 || y >= CELL_API_HB_ROWS) {
        return;
    }
    g_halfblock[y][x] = on ? 1 : 0;
}

void
half_block_rect(int x, int y, int w, int h, int on)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    int x0 = x;
    int y0 = y;
    int x1 = x + w; /* exclusive */
    int y1 = y + h; /* exclusive */

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > CELL_API_HB_COLS) x1 = CELL_API_HB_COLS;
    if (y1 > CELL_API_HB_ROWS) y1 = CELL_API_HB_ROWS;

    uint8_t v = on ? 1 : 0;
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            g_halfblock[yy][xx] = v;
        }
    }
}

void
half_block_clear(void)
{
    memset(g_halfblock, 0, sizeof(g_halfblock));
}

/* --- Frame control -------------------------------------------------- */

void
present(void)
{
    /* No-op in the standalone TU. The kn86-nosh-tb runtime will own
     * the backbuffer -> termbox flush in a follow-up story. Keeping
     * the termbox dependency out of this TU is deliberate: the
     * spike tests the API boundary, not the wiring. */
}

void
yield(void)
{
    /* No-op in the standalone TU. The runtime will pump termbox
     * events between yield() calls when present. */
}

/* --- Inspection + runtime entries ----------------------------------- */

unsigned
cell_chrome_violations(void)
{
    return g_chrome_violation_count;
}

void
cell_api_reset(void)
{
    memset(g_cells, 0, sizeof(g_cells));
    memset(g_halfblock, 0, sizeof(g_halfblock));
    g_chrome_violation_count = 0;
}

void
cell_chrome_write_unchecked(int col, int row, uint32_t ch,
                            cell_color_t fg, cell_color_t bg)
{
    if (row < 0 || row >= CELL_API_ROWS_TOTAL) {
        return;
    }
    if (!col_in_range(col)) {
        return;
    }
    g_cells[row][col].ch = ch;
    g_cells[row][col].fg = fg;
    g_cells[row][col].bg = bg;
}

uint32_t
cell_peek_ch(int col, int row)
{
    if (row < 0 || row >= CELL_API_ROWS_TOTAL) return 0;
    if (!col_in_range(col)) return 0;
    return g_cells[row][col].ch;
}

cell_color_t
cell_peek_fg(int col, int row)
{
    if (row < 0 || row >= CELL_API_ROWS_TOTAL) return CELL_BLACK;
    if (!col_in_range(col)) return CELL_BLACK;
    return g_cells[row][col].fg;
}

cell_color_t
cell_peek_bg(int col, int row)
{
    if (row < 0 || row >= CELL_API_ROWS_TOTAL) return CELL_BLACK;
    if (!col_in_range(col)) return CELL_BLACK;
    return g_cells[row][col].bg;
}

int
half_block_peek(int x, int y)
{
    if (x < 0 || x >= CELL_API_HB_COLS) return 0;
    if (y < 0 || y >= CELL_API_HB_ROWS) return 0;
    return (int)g_halfblock[y][x];
}
