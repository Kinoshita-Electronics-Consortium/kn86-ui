; system-image/lib/ui/compositing.lsp
;
; ui/ — Compositing + overlay primitives (GWP-510).
;
; The depth cues (ui-design-language.md §8.1), the selection primitive
; (§8.2 / §4), and the three composed overlay primitives modal / popup /
; palette (§8.3, §9). Every system-tier overlay — IntelliSense, command
; palette, dialogs, form panels — bottoms out in `ui/modal`.
;
; All draw through render/* (sys_render.c); system-tier only. Cell coords
; throughout (converted to pixels at the render/fill-rect / render/text
; seam). The §4 rule holds: selection = inversion, never a second colour.

; ===========================================================
;  select-row — invert a span (§8.2 / §4 selection channel)
; ===========================================================

; (ui/select-row cx cy cw) -> nil
;   Paint a solid phosphor-hue bar `cw` cells wide at cell row cy (one
;   cell tall). Text drawn over it with fg=UI-BLACK reads as the §4
;   selection channel (black-on-phosphor). The list-selection primitive.
(defn ui/select-row (cx cy cw)
  (render/fill-rect (* cx 8) (* cy 8) (* cw 8) 8 UI-LIT))

; ===========================================================
;  scrim — dither depth cue (§8.1)
; ===========================================================

; (ui/scrim cx cy cw ch) -> nil
;   Stamp the diagonal-dither cell (UI-SHADE-MED, the 0xAA/0x55
;   checkerboard) across the cw x ch cell region so it reads as
;   "dimmed / behind" — the depth cue a modal is laid over (§8.1).
;
;   DEVIATION (documented): §8.1 specifies a per-pixel XOR of the dither
;   mask against whatever is underneath, which screens the base back while
;   keeping it legible. The render/* tier exposes no per-pixel
;   read-modify-write (no set-pixel / get-pixel), so this Lisp scrim
;   OVER-PAINTS the dither cell instead of XOR-ing. In the modal
;   composition flow that is equivalent — the modal clears its own
;   interior next, and the scrim's job is the dimmed texture behind the
;   drop shadow, which the dither cell delivers. A true XOR scrim needs a
;   render-tier bind of composite_scrim (composite.c already implements
;   it); that bind is out of scope for this kit. See report.
(defn ui/scrim (cx cy cw ch)
  (dotimes (r ch)
    (dotimes (c cw)
      (render/glyph (+ cx c) (+ cy r) UI-SHADE-MED 1 UI-LIT UI-BLACK))))

; ===========================================================
;  shadow — drop shadow depth cue (§8.1)
; ===========================================================

; (ui/shadow cx cy cw ch) -> nil
;   A 1-cell solid-black drop shadow for a frame whose top-left cell is
;   (cx,cy), cw x ch cells. The frame's silhouette offset +1 cell right
;   and +1 cell down: an 8px-wide vertical strip down the right edge and
;   an 8px-tall horizontal strip along the bottom, both in the +1/+1
;   position. Reads as a shadow only over a scrim'd base (§8.1) — the
;   scrim supplies the contrast.
(defn ui/shadow (cx cy cw ch)
  ; Right strip: one cell column, from (cx+cw, cy+1) down ch cells.
  (render/fill-rect (* (+ cx cw) 8) (* (+ cy 1) 8) 8 (* ch 8) UI-BLACK)
  ; Bottom strip: one cell row, from (cx+1, cy+ch) right cw cells.
  (render/fill-rect (* (+ cx 1) 8) (* (+ cy ch) 8) (* cw 8) 8 UI-BLACK))

; ===========================================================
;  modal — the workhorse overlay primitive (§8.3)
; ===========================================================

; (ui/modal cx cy cw ch title body-fn) -> nil
;   The composed overlay primitive every dialog/palette/popup is built
;   from (§8.3). In order:
;     1. clear the interior to black (overwrite whatever was there),
;     2. draw the drop shadow (solid black, +1 cell offset),
;     3. draw a double-line cbox with the §6.1 title cutout,
;     4. call body-fn to draw the modal's contents.
;   Pre-condition: the caller has applied (ui/scrim …) to the underlying
;   region (or the whole screen) — the shadow only reads over a scrim.
;   `body-fn` is a thunk of no arguments; pass nil for an empty modal.
(defn ui/modal (cx cy cw ch title body-fn)
  (render/fill-rect (* cx 8) (* cy 8) (* cw 8) (* ch 8) UI-BLACK)
  (ui/shadow cx cy cw ch)
  (ui/cbox cx cy cw ch title 'double)
  (when body-fn (body-fn)))

; ===========================================================
;  Interior list helper (shared by popup / palette / dialog)
; ===========================================================

; (ui/draw-items cx cy cw items selected) -> nil
;   Render `items` (a list of strings) as a vertical list starting at cell
;   (cx,cy), each row scale-1. The row whose 0-based index = `selected` is
;   inverted (the span filled lit, the text drawn black) — the §4 / §8.2
;   selection channel. `selected` nil selects no row (scrollback usage).
(defn ui/draw-items (cx cy cw items selected)
  (let i 0)
  (dolist (it items)
    (let row (+ cy i))
    (if (is i selected)
        (do (ui/select-row cx row cw)
            (render/text (* cx 8) (* row 8) 1 UI-BLACK it))
        (render/text (* cx 8) (* row 8) 1 UI-LIT it))
    (set i (+ i 1))))

; ===========================================================
;  popup — cursor-anchored modal (§9.2)
; ===========================================================

; (ui/popup anchor items signature) -> nil
;   A modal anchored to a position. `anchor` is a (col . row) cell pair
;   (typically the cursor). `items` is a list of strings rendered as a
;   list inside the modal, the first row selected (inverted). `signature`
;   is an optional string docked one row below the modal as a borderless
;   tooltip (§9.2); pass nil for none. Used for IntelliSense completion.
;
;   The modal is sized to the longest item (+ frame + 1-cell interior pad)
;   and as tall as the item count (+ 2 for the frame).
(defn ui/popup (anchor items signature)
  (let ax (car anchor))
  (let ay (cdr anchor))
  (let widest (fold-left (fn (m s) (max m (string-length s))) 1 items))
  (let cw (+ widest 4))
  (let ch (+ (length items) 2))
  (ui/modal ax ay cw ch nil
    (fn () (ui/draw-items (+ ax 2) (+ ay 1) (- cw 4) items 0)))
  (when signature
    (render/text (* (+ ax 2) 8) (* (+ ay ch) 8) 1 UI-LIT signature)))

; ===========================================================
;  palette — centered command palette (§9.1)
; ===========================================================

; (ui/palette items prompt) -> nil
;   The TERM command palette (§9.1): a centered modal, ~64 cells wide and
;   tall enough for the items + chrome. Layout, top to bottom:
;     - prompt row (scale 2): `> ` + prompt text,
;     - separator (single-line rule across the interior),
;     - ranked items list (scale 1), first row selected/inverted, each
;       carrying a ► leading glyph in the selected row,
;     - keybinding footer (scale 1): the ENTER/ESC/TAB hint strip.
;   The caller scrims the underlying screen first (§8.3 pre-condition).
(defn ui/palette (items prompt)
  (let cw 64)
  (let ch (+ (length items) 6))
  (let cx (/ (- 128 cw) 2))
  (let cy (/ (- 75 ch) 2))
  (ui/modal cx cy cw ch nil
    (fn ()
      (let ix (+ cx 2))
      ; prompt row, scale 2 (16 px tall -> 2 cell rows).
      (render/text (* ix 8) (* (+ cy 1) 8) 2 UI-LIT (str "> " prompt))
      ; separator rule across the interior, one row below the scale-2
      ; prompt (which occupies rows cy+1..cy+2).
      (ui/rule ix (+ cy 3) (- cw 4))
      ; ranked items, selected (first) row inverted with a ► lead glyph.
      (let i 0)
      (dolist (it items)
        (let row (+ cy 4 i))
        (if (is i 0)
            (do (ui/select-row ix row (- cw 4))
                (render/glyph ix row UI-BATCH 1 UI-BLACK UI-LIT)
                (render/text (* (+ ix 2) 8) (* row 8) 1 UI-BLACK it))
            (render/text (* (+ ix 2) 8) (* row 8) 1 UI-LIT it))
        (set i (+ i 1)))
      ; footer hint strip on the last interior row.
      (render/text (* ix 8) (* (+ cy ch -2) 8) 1 UI-LIT
        "ENTER run  ESC cancel  TAB next"))))
