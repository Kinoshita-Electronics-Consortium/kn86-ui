; system-image/lib/ui/lists.lsp
;
; ui/ — Lists, trees, tables (GWP-511).
;
; The data-navigation widgets (ui-design-language.md §10.3). All draw
; through render/* (sys_render.c); system-tier only. The §4 four-channel
; rule governs: selection = inversion (never a second colour), identity =
; glyph (leading-glyph column, tree disclosure, typed-column header),
; magnitude = density (the data primitives in ui/data.lsp).
;
; Cell coords throughout. The selected row inverts via ui/select-row
; (compositing.lsp) + text drawn in the inverted pair (fg=UI-BLACK).

; ===========================================================
;  list — vertical scrolling list (§10.3)
; ===========================================================

; (ui/list-row cx cy cw text glyph selected) -> nil
;   Draw one list row at cell (cx,cy): a leading-glyph column (`glyph`, the
;   §4 attention channel — ● unread / * pinned / ► batch / space normal),
;   then the row text at cx+2. When `selected` is non-nil the whole span is
;   inverted (the §4 selection channel): the row is filled lit, the lead
;   glyph and text drawn black-on-phosphor.
(defn ui/list-row (cx cy cw text glyph selected)
  (if selected
      (do (ui/select-row cx cy cw)
          (render/glyph cx cy glyph 1 UI-BLACK UI-LIT)
          (render/text (* (+ cx 2) 8) (* cy 8) 1 UI-BLACK text))
      (do (render/glyph cx cy glyph 1 UI-LIT UI-BLACK)
          (render/text (* (+ cx 2) 8) (* cy 8) 1 UI-LIT text))))

; (ui/list cx cy cw ch rows selected leading-glyph-fn) -> nil
;   A vertical scrolling list in the cw x ch cell rectangle. `rows` is a
;   list of strings; `selected` is the 0-based index of the inverted row
;   (nil = none — the scrollback / log usage: a list with no selection is
;   a log pane); `leading-glyph-fn` is called with (index row-text) and
;   returns the §4 leading-glyph code for that row (pass nil for a constant
;   space column). At most `ch` rows render (the visible window); callers
;   that scroll pass an already-windowed `rows` + an adjusted `selected`.
;
;   Scrollback / log usage: `(ui/list cx cy cw ch log-lines nil nil)` —
;   no selection, space lead column — renders an append-only log pane.
(defn ui/list (cx cy cw ch rows selected leading-glyph-fn)
  (let i 0)
  (dolist (row rows)
    (when (< i ch)
      (let g (if leading-glyph-fn (leading-glyph-fn i row) UI-SPACE))
      (ui/list-row cx (+ cy i) cw row g (is i selected)))
    (set i (+ i 1))))

; ===========================================================
;  tree — collapsible tree / s-expression browser (§10.3)
; ===========================================================

; A tree node is (label . children); a leaf is (label) or (label . nil).
; `expanded?` is a predicate of a node -> truthy when its children show.
; `select` is the node currently selected (inverted when drawn); pass nil
; for no selection. Disclosure glyph: ► collapsed (has hidden children),
; ▼ expanded, space for a leaf.

; (ui/tree-node node cx cy cw depth expanded? select) -> next-row
;   Render `node` at row cy, indented by `depth` cells, and (if expanded)
;   its children below. Returns the next free row after the subtree, so a
;   caller can stack siblings. Indent is leading cells + the disclosure
;   glyph (the §4 identity channel for open/closed).
(defn ui/tree-node (node cx cy cw depth expanded? select)
  (let label (car node))
  (let kids (cdr node))
  (let open (expanded? node))
  (let disc (cond ((nil? kids) UI-SPACE)
                  (open UI-EXPANDED)
                  (else UI-COLLAPSED)))
  (let ind (* depth 2))
  (let sel (is node select))
  ; disclosure glyph + label at the indented column.
  (if sel
      (do (ui/select-row cx cy cw)
          (render/glyph (+ cx ind) cy disc 1 UI-BLACK UI-LIT)
          (render/text (* (+ cx ind 2) 8) (* cy 8) 1 UI-BLACK label))
      (do (render/glyph (+ cx ind) cy disc 1 UI-LIT UI-BLACK)
          (render/text (* (+ cx ind 2) 8) (* cy 8) 1 UI-LIT label)))
  ; descend into children when expanded.
  (let row (+ cy 1))
  (when open
    (dolist (kid kids)
      (set row (ui/tree-node kid cx row cw (+ depth 1) expanded? select))))
  row)

; (ui/tree cx cy cw ch root expanded? select) -> nil
;   Render the tree rooted at `root` in the cw x ch rectangle. `expanded?`
;   is the open-state predicate, `select` the selected node (inverted).
;   The same widget backs nEmacs's s-expression browser and reader-mode
;   content (§10.3). `ch` is advisory — the caller windows large trees.
(defn ui/tree (cx cy cw ch root expanded? select)
  (ui/tree-node root cx cy cw 0 expanded? select))

; ===========================================================
;  table — pinned header + frozen cols + typed-column glyphs (§10.3)
; ===========================================================

; A column is (label . width) in cells. `header-glyphs` is a parallel list
; of typed-column glyph codes ($ % @ # …, the §4 identity channel for a
; column's kind), one per column (UI-SPACE for an untyped column). `rows`
; is a list of cell-string lists (one per column). `frozen-cols` is the
; count of leftmost columns pinned during horizontal scroll — carried as
; the scroll contract; a static render draws every column in order.

; (ui/clip txt w) -> string  — txt clipped to at most w characters.
(defn ui/clip (txt w)
  (substring txt 0 (min (string-length txt) (max 0 w))))

; (ui/table-cells cx cy cols cells fg) -> nil
;   Render one row of `cells` across `cols` (each (label . width)) at row
;   cy, in the (fg, black) colour pair, each cell clipped to its width.
(defn ui/table-cells (cx cy cols cells fg)
  (let col cx)
  (let cs cells)
  (dolist (c cols)
    (let w (cdr c))
    (render/text (* col 8) (* cy 8) 1 fg (ui/clip (if cs (car cs) "") w))
    (set col (+ col w 1))
    (set cs (if cs (cdr cs) nil))))

; (ui/table cx cy cw ch cols rows frozen-cols header-glyphs) -> nil
;   A table: a pinned header row (inverted — the §4 emphasis channel marks
;   it as chrome), then the body rows. Each header label is prefixed with
;   its typed-column glyph from `header-glyphs` (the §4 identity channel
;   for a column's kind: $ currency, % percent, @ address, # count).
;   `frozen-cols` columns are the leftmost pinned set (the horizontal-
;   scroll contract; a static render draws all columns in order).
(defn ui/table (cx cy cw ch cols rows frozen-cols header-glyphs)
  ; Header row: filled lit, labels drawn black, each with its type glyph.
  (ui/select-row cx cy cw)
  (let col cx)
  (let hg header-glyphs)
  (dolist (c cols)
    (let w (cdr c))
    (let g (if hg (car hg) UI-SPACE))
    (render/glyph col cy g 1 UI-BLACK UI-LIT)
    (render/text (* (+ col 1) 8) (* cy 8) 1 UI-BLACK (ui/clip (car c) (- w 1)))
    (set col (+ col w 1))
    (set hg (if hg (cdr hg) nil)))
  ; Body rows beneath the header, normal pair, windowed to ch-1 rows.
  (let i 0)
  (dolist (r rows)
    (when (< i (- ch 1))
      (ui/table-cells cx (+ cy 1 i) cols r UI-LIT))
    (set i (+ i 1))))
