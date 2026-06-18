/*
 * cell_api.h — The constrained KN-86 cell API (ADR-0027).
 *
 * This is the ONLY header a cart-side render surface includes for
 * rendering. The 10 builtins below are the entire cartridge-facing
 * draw surface in kn86-nosh-tb. Cartridges cannot:
 *   - call termbox primitives directly,
 *   - clear the whole screen (chrome rows are runtime-owned),
 *   - manipulate the cursor or input/output modes,
 *   - take over presentation.
 *
 * Grid contract (ADR-0027):
 *   - 128 columns x 75 rows on tty1 at 8x8 PSF.
 *   - Row 0: nOSh chrome (status). Owned by the runtime.
 *   - Row 74: nOSh chrome (action). Owned by the runtime.
 *   - Rows 1..73: cart-usable region (73 rows).
 *
 * Half-block sub-pixel canvas (cart-usable only):
 *   - 128 columns x 146 rows of sub-pixels.
 *   - Sub-row 0 = upper half of cell row 1, sub-row 1 = lower half
 *     of cell row 1, etc. (2 sub-rows per cell row x 73 usable rows).
 *
 * Chrome enforcement: any cell_set / cell_print / half_block_*
 * targeting row 0, row 74, or otherwise outside the cell grid is
 * silently dropped and increments the cart_chrome_violation counter
 * (read via cell_chrome_violations()).
 *
 * The 10 builtins (the closed list for GWP-382/S3):
 *   cell_set, cell_print, cell_clear_cart_region, cell_cols,
 *   cell_rows_usable, half_block_set, half_block_rect,
 *   half_block_clear, present, yield.
 *
 * Anything beyond those 10 is either runtime-internal (declared
 * separately below as `_unchecked` / inspection helpers) or out of
 * scope for the spike.
 */

#ifndef KN86_CELL_API_H
#define KN86_CELL_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Sanctioned color set -------------------------------------------- */

/* The KN-86 palette is amber on black, full stop. The runtime maps
 * these tokens to termbox attributes inside cell_api.c so cartridges
 * never see termbox color encodings. */
typedef uint16_t cell_color_t;

#define CELL_AMBER  ((cell_color_t)1)
#define CELL_BLACK  ((cell_color_t)0)

/* --- The 10 builtins (closed list for GWP-382/S3) -------------------- */

/* Write a single cell. Drops + increments cart_chrome_violation if
 * row is 0, 74, or otherwise outside [0..74]. Out-of-range columns
 * are silently clipped without a violation. */
void cell_set(int col, int row, uint32_t ch, cell_color_t fg, cell_color_t bg);

/* Write a NUL-terminated string at (col, row), clipping at the right
 * edge of the grid (no wrap). Drops + increments
 * cart_chrome_violation once if the row is chrome / out-of-range. */
void cell_print(int col, int row, cell_color_t fg, cell_color_t bg, const char *str);

/* Clear rows 1..73. Never touches chrome rows. Does not increment
 * cart_chrome_violation (this is a runtime-blessed operation when
 * called by carts via the FFI). */
void cell_clear_cart_region(void);

/* Returns 128 (full grid columns). */
int cell_cols(void);

/* Returns 73 (number of cart-usable rows: rows 1..73 inclusive). */
int cell_rows_usable(void);

/* Set a half-block sub-pixel within the cart-usable canvas
 * (128 x 146). Out-of-canvas writes are silently ignored without
 * incrementing cart_chrome_violation; chrome rows are unreachable
 * from this primitive by construction. `on` is non-zero to light
 * the sub-pixel, zero to clear. */
void half_block_set(int x, int y, int on);

/* Filled rectangle in half-block sub-pixels. Clips to the cart
 * canvas. Negative origins and over-large dimensions are handled
 * by clipping, not by counter increments. */
void half_block_rect(int x, int y, int w, int h, int on);

/* Clears the cart-usable half-block canvas only. Cell grid
 * (text + chrome) is untouched. */
void half_block_clear(void);

/* Marks the frame ready. The runtime may auto-present at frame
 * boundaries; cartridges should treat present() as advisory. In the
 * stand-alone test build present() is a no-op against the
 * backbuffer. */
void present(void);

/* Returns control to the runtime so it can poll input and dispatch
 * events. In the stand-alone test build yield() is a no-op. */
void yield(void);

/* --- Runtime + test inspection (NOT exposed to cart Fe FFI) --------- */

/* Counter for chrome-row violations and out-of-grid row writes.
 * Monotonic since the last cell_api_reset(). */
unsigned cell_chrome_violations(void);

/* Reset the cell grid, half-block canvas, and chrome violation
 * counter. Tests use this between cases; the runtime calls it
 * during init and on cart unload. */
void cell_api_reset(void);

/* Runtime-side chrome write. Bypasses the cart gate; used only by
 * nOSh's status / action row renderers, never reachable from carts.
 * Out-of-grid writes are dropped silently (no counter touched). */
void cell_chrome_write_unchecked(int col, int row, uint32_t ch,
                                 cell_color_t fg, cell_color_t bg);

/* Test / debug accessors into the backbuffer. Public for
 * test_cell_api.c; the cart FFI binder MUST NOT expose these. */
uint32_t     cell_peek_ch(int col, int row);
cell_color_t cell_peek_fg(int col, int row);
cell_color_t cell_peek_bg(int col, int row);
int          half_block_peek(int x, int y);

/* Grid + canvas dimensions, exposed as compile-time constants for
 * any runtime code that needs to size its own buffers. */
#define CELL_API_COLS              128
#define CELL_API_ROWS_TOTAL        75
#define CELL_API_CHROME_ROW_TOP     0
#define CELL_API_CHROME_ROW_BOTTOM 74
#define CELL_API_ROWS_USABLE       73
#define CELL_API_HB_COLS           128
#define CELL_API_HB_ROWS           146  /* 2 * 73 */

#ifdef __cplusplus
}
#endif

#endif /* KN86_CELL_API_H */
