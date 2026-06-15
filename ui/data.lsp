; system-image/lib/ui/data.lsp
;
; ui/ — Data-display primitives (GWP-511).
;
; The magnitude channel (ui-design-language.md §4 channel 3, §10.4): the
; vertical-bar sparkline and the horizontal fine-bar both encode HOW MUCH
; of a thing exists via the block-density ramp — never category, never a
; second colour. All draw through render/* (sys_render.c); system-tier
; only.
;
;   sparkline -> the vertical bar ramp 0x96..0x9D (▁..█), 8 sub-cell steps.
;   fine-bar  -> the horizontal left-fill ramp 0xA8..0xAE + 0xA2 (█), 8×
;                horizontal sub-cell resolution.
;
; (status-glyph lives in ui/frames.lsp with the other §4 identity glyphs.)

; ===========================================================
;  sparkline — time-series via vbars (§10.4)
; ===========================================================

; (ui/spark-step v hi) -> 0..8
;   Quantise value `v` against ceiling `hi` to an 8-step bar height. 0 maps
;   to an empty cell, anything positive to at least 1 (so a non-zero
;   sample is always visible), the ceiling to a full block.
(defn ui/spark-step (v hi)
  (cond ((<= v 0) 0)
        ((<= hi 0) 0)
        ((<= hi v) 8)
        (else (max 1 (floor (/ (* v 8) hi))))))

; (ui/sparkline cx cy cw data) -> nil
;   A single-row sparkline: each value in `data` becomes one vbar glyph
;   (0x96..0x9D) at scale 1, left to right from cell (cx,cy). The ceiling
;   is the data max (the logradar recipe soft-caps at 3× mean; callers
;   that want that pre-scale `data`). At most `cw` cells render; when the
;   series is longer the NEWEST `cw` samples show (rightmost = newest, the
;   logradar newest-bolded convention — the tail of the list is newest).
(defn ui/sparkline (cx cy cw data)
  (let hi (fold-left (fn (m v) (max m v)) 1 data))
  ; window to the last cw samples (newest), preserving order.
  (let n (length data))
  (let shown (if (< cw n) (drop data (- n cw)) data))
  (let col cx)
  (dolist (v shown)
    (render/glyph col cy (ui/vbar (ui/spark-step v hi)) 1 UI-LIT UI-BLACK)
    (set col (+ col 1))))

; ===========================================================
;  fine-bar — horizontal value bar (§10.4)
; ===========================================================

; (ui/fine-bar cx cy cw value max) -> nil
;   A horizontal progress / value bar `cw` cells wide with sub-cell
;   precision: 8× horizontal resolution via the left-fill ramp
;   (0xA8..0xAE) plus the full block (0xA2). value/max is rendered as a
;   fill of (value/max * cw*8) eighths: each whole 8-eighth chunk is a full
;   block, the remainder cell is the matching partial hbar, the rest of the
;   bar is empty. Clamped to [0, max]; max<=0 renders an empty bar.
(defn ui/fine-bar (cx cy cw value max)
  (let v (cond ((< value 0) 0) ((< max value) max) (else value)))
  ; total eighths of fill across the whole cw-cell bar.
  (let eighths (if (<= max 0) 0 (floor (/ (* v cw 8) max))))
  (let full (floor (/ eighths 8))) ; whole full-block cells
  (let rem (- eighths (* full 8))) ; remainder eighths in the next cell
  (let col cx)
  (let i 0)
  (while (< i cw)
    (cond ((< i full)
           (render/glyph col cy UI-FULL 1 UI-LIT UI-BLACK))
          ((and (is i full) (< 0 rem))
           (render/glyph col cy (ui/hbar rem) 1 UI-LIT UI-BLACK))
          (else
           (render/glyph col cy UI-HBAR-0 1 UI-LIT UI-BLACK)))
    (set col (+ col 1))
    (set i (+ i 1))))
