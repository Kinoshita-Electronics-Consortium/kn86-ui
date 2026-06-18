; system-image/lib/ui/input.lsp
;
; ui/ — Navigation + input + transition primitives (GWP-512).
;
; The interactive widgets (ui-design-language.md §10.5) plus the transition
; wrappers (§10.7). All draw through render/* (sys_render.c); system-tier
; only. The §4 four-channel rule governs: focus = inversion (the focused
; field / tab / cursor cell is the inverted pair), identity = glyph,
; magnitude = the sub-cell fill of the continuous controls. No second
; colour, ever.

; ===========================================================
;  tab-bar — numbered tab strip (§10.5)
; ===========================================================

; (ui/tab-bar cx cy tabs selected) -> nil
;   A numbered tab strip (1..N, matching the Bare Deck STATUS/CIPHER/LAMBDA
;   /LINK/SYS pattern). `tabs` is a list of label strings; `selected` is
;   the 0-based selected index. Each tab renders as `n:LABEL`; the selected
;   tab's span is inverted (the §4 selection channel). Tabs are laid out
;   left to right with a one-cell gap.
(defn ui/tab-bar (cx cy tabs selected)
  (let col cx)
  (let i 0)
  (dolist (label tabs)
    (let cell (str (number->string (+ i 1)) ":" label))
    (let w (string-length cell))
    (if (is i selected)
        (do (ui/select-row col cy w)
            (render/text (* col 8) (* cy 8) 1 UI-BLACK cell))
        (render/text (* col 8) (* cy 8) 1 UI-LIT cell))
    (set col (+ col w 1))
    (set i (+ i 1))))

; ===========================================================
;  field — single-line editable + caret (NEW)
; ===========================================================

; (ui/field cx cy cw text caret) -> nil
;   The minibuffer / echo / prompt atom: a single-line editable text field
;   `cw` cells wide at (cx,cy). `text` is the field contents (clipped to
;   cw); `caret` is the 0-based cell column of the insertion point, or nil
;   for a read-only echo (no caret). The caret cell is inverted (the §4
;   focus channel) — the glyph under the caret reads black-on-phosphor; an
;   empty caret cell is a solid lit block.
(defn ui/field (cx cy cw text caret)
  (let shown (substring text 0 (min (string-length text) cw)))
  (render/text (* cx 8) (* cy 8) 1 UI-LIT shown)
  (when (and caret (< caret cw))
    ; the character under the caret (space past the end of text).
    (let ch (if (< caret (string-length shown))
                (string-ref shown caret)
                32))
    (ui/select-row (+ cx caret) cy 1)
    (render/glyph (+ cx caret) cy ch 1 UI-BLACK UI-LIT)))

; ===========================================================
;  form — multi-field input panel (§10.5)
; ===========================================================

; (ui/form cx cy cw fields focused-field) -> nil
;   A multi-field input panel. `fields` is a list of (label . value)
;   pairs, each rendered on its own row: the label, then an inline field
;   (ui/field) for the value. `focused-field` is the 0-based index of the
;   focused field; the focused field's value is inverted (the §4 focus
;   channel) and carries a caret at the end of its value. TAB cycling is
;   the caller's job (it re-renders with the next focused-field); the
;   component draws the current focus state.
;
;   Layout: each field is `label: value` on row cy+i. The value column
;   starts at a fixed offset (the longest label + 2) so values align.
(defn ui/form (cx cy cw fields focused-field)
  (let label-w (fold-left (fn (m f) (max m (string-length (car f)))) 1 fields))
  (let val-col (+ cx label-w 2))
  (let val-w (- cw label-w 2))
  (let i 0)
  (dolist (f fields)
    (let row (+ cy i))
    (render/text (* cx 8) (* row 8) 1 UI-LIT (str (car f) ":"))
    (if (is i focused-field)
        ; focused: the value field shows a caret at the end (the §4 focus
        ; channel). caret = value length (insertion point past the text).
        (ui/field val-col row val-w (cdr f) (string-length (cdr f)))
        (ui/field val-col row val-w (cdr f) nil))
    (set i (+ i 1))))

; ===========================================================
;  slider — horizontal continuous control (§10.5)
; ===========================================================

; (ui/slider cx cy cw value min max) -> nil
;   A horizontal bar `cw` cells wide with a cursor at the value position.
;   The track is the box-drawing horizontal line; the cursor cell — at
;   column round((value-min)/(max-min) * (cw-1)) — is inverted (the §4
;   focus channel reads as "here is the value"). Sub-cell precision: the
;   cursor lands on the nearest cell to the continuous value.
(defn ui/slider (cx cy cw value min max)
  (let span (- max min))
  (let frac (if (<= span 0) 0 (/ (- value min) span)))
  (let pos (floor (+ (* frac (- cw 1)) 0.5)))   ; round to nearest cell
  (let pos2 (cond ((< pos 0) 0) ((< (- cw 1) pos) (- cw 1)) (else pos)))
  ; track line first.
  (dotimes (i cw)
    (render/glyph (+ cx i) cy UI-BOX-H 1 UI-LIT UI-BLACK))
  ; cursor cell inverted (a solid lit block with the full-block glyph).
  (ui/select-row (+ cx pos2) cy 1))

; ===========================================================
;  dial — circular continuous control (§10.5)
; ===========================================================

; (ui/dial cx cy cw ch value min max) -> nil
;   A continuous-control indicator in the cw x ch cell box with a value
;   readout. The fill proportion (value within [min,max]) is shown as a
;   bottom-anchored vertical fill across the box interior, drawn at half-
;   cell (4 px) vertical resolution via render/fill-rect — the "half-block
;   sub-pixel" precision §10.5 calls for, on the render surface. The
;   numeric value is printed at the box's top-left interior cell.
(defn ui/dial (cx cy cw ch value min max)
  (ui/cbox cx cy cw ch nil 'single)
  (let span (- max min))
  (let frac (if (<= span 0) 0 (/ (- value min) span)))
  (let frac2 (cond ((< frac 0) 0) ((< 1 frac) 1) (else frac)))
  ; interior is (cw-2) x (ch-2) cells; fill height in half-cells (4 px).
  (let ih (- ch 2))
  (let half-cells (* ih 2))                       ; total 4-px steps
  (let filled (floor (+ (* frac2 half-cells) 0.5)))
  (let fill-px (* filled 4))
  (let ix (* (+ cx 1) 8))
  (let iw (* (- cw 2) 8))
  (let ibot (* (+ cy ch -1) 8))                   ; bottom interior pixel row
  (render/fill-rect ix (- ibot fill-px) iw fill-px UI-LIT)
  ; numeric readout at the top interior cell, in the inverted pair so it
  ; stays legible over any fill.
  (render/text (* (+ cx 1) 8) (* (+ cy 1) 8) 1 UI-BLACK
               (number->string value)))

; ===========================================================
;  cycler — single-row enum cycler (§10.5)
; ===========================================================

; (ui/cycler cx cy label current options) -> nil
;   Cycle through a small enum on one row: `LABEL: A > B > C`, the option
;   matching `current` inverted (the §4 selection channel). `options` is a
;   list of strings; `current` is the selected option string. The SYS-tab
;   aesthetic-picker primitive (MODE: AMBER > WHITE > GREEN).
(defn ui/cycler (cx cy label current options)
  (render/text (* cx 8) (* cy 8) 1 UI-LIT (str label ": "))
  (let col (+ cx (string-length label) 2))
  (let i 0)
  (dolist (opt options)
    ; from the second option on, a ► separator occupies one cell — it IS
    ; the inter-option gap (no extra spacer).
    (when (< 0 i)
      (render/glyph col cy UI-COLLAPSED 1 UI-LIT UI-BLACK)
      (set col (+ col 1)))
    (let w (string-length opt))
    (if (is opt current)
        (do (ui/select-row col cy w)
            (render/text (* col 8) (* cy 8) 1 UI-BLACK opt))
        (render/text (* col 8) (* cy 8) 1 UI-LIT opt))
    (set col (+ col w))
    (set i (+ i 1))))

; ===========================================================
;  toast — transient auto-dismissing band (NEW)
; ===========================================================

; (ui/toast text style) -> nil
;   A transient notification band: a centered inverted strip carrying
;   `text`, mounted just above the Row-74 action bar. `style` is an
;   advisory symbol ('info / 'warn / 'ok) reserved for a future
;   leading-glyph (the §4 identity channel); v0.1 renders the band
;   identically for every style. The toast captures NO focus and has NO
;   border — it is a passive band; the TIMED LIFECYCLE (auto-dismiss after
;   a few seconds) is the caller's responsibility (mount on an event,
;   unmount on an on-tick timer). The component only draws the band.
;
;   The band is sized to the text + a one-cell pad each side and centered
;   horizontally; it sits two cell rows above the action bar (row 72 at
;   the 1× view), inverted so it reads as an over-content notification.
(defn ui/toast (text style)
  (let w (+ (string-length text) 2))
  (let cx (floor (/ (- 128 w) 2)))
  (let cy 72)
  (ui/select-row cx cy w)
  (render/text (* (+ cx 1) 8) (* cy 8) 1 UI-BLACK text))

; ===========================================================
;  reveal / unreveal — transition wrappers (§10.7, ADR-0033)
; ===========================================================

; (ui/reveal style) / (ui/unreveal style) -> nil
;   The canonical transition primitive (ADR-0033 / reveal-styles.md): a
;   Fe-side wrapper over the (reveal …) / (unreveal …) NoshAPI primitive.
;   Modal entry/exit, cart load, and ambient-mode entry/exit all dispatch
;   through it. `style` is a reveal-style keyword (e.g. ':char-flicker',
;   ':radial', ':no-more-secrets') passed straight through.
;
;   REACHABILITY (documented dependency): the underlying `reveal` /
;   `unreveal` primitives are bound by the cart/runtime FFI bridge
;   (src/nosh_lisp_bridge.c), NOT by the System-tier render bind
;   (sys_render.c / sys_context.c). A System context created by
;   nosh_system_context_create therefore has NO `reveal` symbol bound, so
;   CALLING ui/reveal there raises "unbound symbol `reveal`" — the same
;   capability boundary the render/* split enforces (the binding-set IS the
;   capability). These wrappers fix the call SHAPE the kit will use; making
;   them live is a follow-on: bind reveal/unreveal into the System tier
;   (an sys_render.c / sys_context.c addition) when the transition surface
;   is wired into system screens. Defining the wrapper does not raise — the
;   unbound symbol only errors when the body is actually evaluated.
(defn ui/reveal (style)
  (reveal ':style style))

(defn ui/unreveal (style)
  (unreveal ':style style))
