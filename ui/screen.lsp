; ui/screen.lsp
;
; ui/ — the screen render engine (Phase 1).
;
; A screen is DATA: a declarative tree of nodes. (ui/draw tree) lays it out
; and paints it — browser-style (tree = DOM, this = layout + paint engine).
; The engine owns layout (block flow: stack, + an `at` absolute escape
; hatch) and paints through the privileged render/* tier. Grid-parametric:
; layout is in glyph units (1 unit = an 8 px cell) with per-element integer
; :scale; physical metrics come from the surface (render/width|height), not
; a baked grid. See docs/plans/2026-06-16-ui-render-engine-design.md.
;
; Node form (Phase 1 canonical): (tag (attrs...) child...)
;   - attrs is a flat symbol-keyed plist, possibly () — e.g. (scale 5 align center)
;   - children are nodes, or a leaf string (text content)
;
; Phase 1 tags: screen, status-bar, action-bar, stack, text, subtitle,
;               headline, rule, spacer, at.

; ===========================================================
;  Node accessors + attr lookup
; ===========================================================

(defn scr/tag   (n) (car n))
(defn scr/attrs (n) (car (cdr n)))
(defn scr/kids  (n) (cdr (cdr n)))

; (scr/attr plist key default) -> value — walk a flat (k v k v ...) plist.
(defn scr/attr (plist key default)
  (cond ((nil? plist) default)
        ((is (car plist) key) (car (cdr plist)))
        (else (scr/attr (cdr (cdr plist)) key default))))

; ===========================================================
;  Metrics (glyph unit = 8 px; chrome rows are scale-2 = 16 px)
; ===========================================================

(defn scr/glyph () 8)
(defn scr/chrome-vpad () 2)                          ; px of phosphor above/below chrome text
; chrome band = scale-2 glyph (16 px) + vpad top + vpad bottom
(defn scr/chrome-h () (+ (* (scr/glyph) 2) (* 2 (scr/chrome-vpad))))
(defn scr/margin () 16)   ; content left inset (2 cells)

; pixel width of `s` drawn at `scale`
(defn scr/text-w (s scale) (* (string-length s) (* (scr/glyph) scale)))

; ===========================================================
;  Chrome — status bar (row 0) + action bar (last row), inverted
; ===========================================================

(defn scr/status-bar (node)
  (let a (scr/attrs node))
  (let handle (scr/attr a 'handle ""))
  (let cr (scr/attr a 'cr ""))
  (let sec (scr/attr a 'sec ""))
  (render/fill-rect 0 0 (render/width) (scr/chrome-h) 1)         ; lit band
  (let ty (scr/chrome-vpad))                                     ; top padding inside the band
  (render/text 4 ty 2 0 (str "KN-86   HANDLE: " handle))         ; black-on-phosphor
  (let right (str "CR " cr (if (is sec "") "" (str "   " sec))))
  (render/text (- (render/width) (scr/text-w right 2) 8) ty 2 0 right))

(defn scr/action-bar (node)
  (let y (- (render/height) (scr/chrome-h)))
  (render/fill-rect 0 y (render/width) (scr/chrome-h) 1)
  (let parts (map (fn (p) (str (car p) "=" (car (cdr p)))) (scr/kids node)))
  (render/text 4 (+ y (scr/chrome-vpad)) 2 0 (join parts "   ")))

; ===========================================================
;  Flow nodes — each returns the next free pixel-y
; ===========================================================

; text / subtitle / headline share one drawer; default scale differs.
(defn scr/text-node (node x y default-scale)
  (let a (scr/attrs node))
  (let scale (scr/attr a 'scale default-scale))
  (let s (car (scr/kids node)))
  (let tx (if (is (scr/attr a 'align 'left) 'center)
              (render/center-x (scr/text-w s scale))
              x))
  (render/text tx y scale 1 s)
  (+ y (* (scr/glyph) scale)))

(defn scr/rule (node x y)
  (let a (scr/attrs node))
  (let w (scr/attr a 'w (- (render/width) (* 2 x))))
  (render/fill-rect x (+ y 3) w 2 1)
  (+ y 10))

(defn scr/spacer (node y)
  (+ y (* (scr/glyph) (scr/attr (scr/attrs node) 'h 1))))

; stack — flow children down with :gap (cells) and :pad (cells)
(defn scr/stack (node x y)
  (let a (scr/attrs node))
  (let gap (* (scr/glyph) (scr/attr a 'gap 0)))
  (let pad (* (scr/glyph) (scr/attr a 'pad 0)))
  (let cx (+ x pad))
  (let cy (+ y pad))
  (dolist (kid (scr/kids node))
    (set cy (+ (scr/node kid cx cy) gap)))
  cy)

; at — absolute escape hatch. (at (x PX y PX scale S) "str") draws and does
; NOT advance flow (returns the incoming y).
(defn scr/at (node y)
  (let a (scr/attrs node))
  (render/text (scr/attr a 'x 0) (scr/attr a 'y 0)
               (scr/attr a 'scale 1) 1 (car (scr/kids node)))
  y)

; dispatch one flow node at (x,y) -> next y
(defn scr/node (node x y)
  (let tag (scr/tag node))
  (cond ((is tag 'stack)    (scr/stack node x y))
        ((is tag 'text)     (scr/text-node node x y 1))
        ((is tag 'subtitle) (scr/text-node node x y 2))
        ((is tag 'headline) (scr/text-node node x y 5))
        ((is tag 'rule)     (scr/rule node x y))
        ((is tag 'spacer)   (scr/spacer node y))
        ((is tag 'at)       (scr/at node y))
        (else y)))

; ===========================================================
;  Entry point
; ===========================================================

; (ui/draw screen) -> nil
;   Paint a screen tree. status-bar pins to row 0, action-bar to the last
;   row; every other top-level child flows down the body region.
(defn ui/draw (screen)
  (render/begin-frame)
  (let body-y (+ (scr/chrome-h) (scr/glyph)))   ; below status bar + 1 cell pad
  (dolist (kid (scr/kids screen))
    (let tag (scr/tag kid))
    (cond ((is tag 'status-bar) (scr/status-bar kid))
          ((is tag 'action-bar) (scr/action-bar kid))
          (else (set body-y (scr/node kid (scr/margin) body-y)))))
  nil)
