; system-image/lib/ui/frames.lsp
;
; ui/ — Frames + chrome rows + headline + status-glyph (GWP-510).
;
; The frame primitives (ui-design-language.md §6, §10.2) and the chrome
; rows (§10.1) every layout panel and every screen is built from. All draw
; through the privileged render/* tier (sys_render.c); the kit is
; system-tier only and never touches the cart cell API.
;
; Coordinate convention (sys_render.h): render/box + render/glyph take CELL
; coords (the primitive multiplies by 8); render/text + render/fill-rect
; take PIXEL coords. The ui/ frame components are cell-addressed and
; convert where they reach a pixel primitive.
;
; The §4 four-channel rule governs everything here: identity = glyph,
; selection = inversion (a black-on-phosphor colour pair via fg=UI-BLACK),
; magnitude = density, attention = border weight / leading-glyph / blink.
; There is never a second colour.

; ===========================================================
;  cbox — the box-drawing primitive (§6 + §6.1 title cutout)
; ===========================================================

; (ui/cbox cx cy cw ch title weight) -> nil
;   The raw framed-region primitive. cx/cy top-left in cells, cw/ch size in
;   cells, title a string or nil, weight 'single or 'double (§6). The §6.1
;   title cutout (clear the title cells + a one-cell pad before stamping,
;   so the border line does not strike through the title) is applied by
;   the render/box C primitive (render.h) when title is non-nil. weight
;   'double draws the heavier frame — the focused-panel / modal weight;
;   only one element on screen is 'double at a time (§6).
(defn ui/cbox (cx cy cw ch title weight)
  (render/box cx cy cw ch (is weight 'double) title))

; ===========================================================
;  panel / box — single-weight cbox wrappers (§10.2)
; ===========================================================

; (ui/panel cx cy cw ch title) -> nil
;   The default frame for every layout panel: a single-line cbox with an
;   optional title cutout. Pass nil for an untitled panel.
(defn ui/panel (cx cy cw ch title)
  (ui/cbox cx cy cw ch title 'single))

; (ui/box cx cy cw ch title) -> nil   — alias for panel (§10.2 lists both).
(defn ui/box (cx cy cw ch title)
  (ui/panel cx cy cw ch title))

; ===========================================================
;  rule — horizontal divider (NEW)
; ===========================================================

; (ui/rule cx cy cw) -> nil
;   A single-line horizontal divider — `cw` cells of the box-drawing
;   horizontal `─` (UI-BOX-H) starting at cell (cx,cy). The separator row
;   inside a panel, palette, or form. Drawn in the normal phosphor pair.
(defn ui/rule (cx cy cw)
  (dotimes (i cw)
    (render/glyph (+ cx i) cy UI-BOX-H 1 UI-LIT UI-BLACK)))

; ===========================================================
;  Inverted-chrome helper
; ===========================================================

; (ui/chrome-band cy-px h-px) -> nil
;   Fill a full-surface-width horizontal band lit (the phosphor hue) at
;   pixel row cy-px, h-px tall. Text drawn over it with fg=UI-BLACK reads
;   as the §4 inverted chrome (black-on-phosphor). Internal to the chrome
;   rows.
(defn ui/chrome-band (cy-px h-px)
  (render/fill-rect 0 cy-px (render/width) h-px UI-LIT))

; ===========================================================
;  status-bar — Row 0 chrome (§10.1)
; ===========================================================

; (ui/status-bar handle credits header) -> nil
;   The runtime-owned Row 0 status strip. Scale-2, inverted. Renders
;   `handle  ► credits  ► header` — operator handle, credit balance, and
;   the active section header, ►-separated (the logradar field-separator
;   pattern). Drawn at pixel row 0; scale-2 chrome is 16 px tall.
;
;   Inversion (§4): the band is filled lit, then the text is drawn with
;   fg=UI-BLACK so each glyph paints black-on-phosphor.
(defn ui/status-bar (handle credits header)
  (ui/chrome-band 0 16)
  (let sep (str "  " (char->string UI-SEP-ARROW) " "))
  (let cur (char->string UI-CURRENCY))
  (let line (str handle sep cur credits sep header))
  (render/text 0 0 2 UI-BLACK line))

; ===========================================================
;  action-bar — Row 74 chrome (§10.1)
; ===========================================================

; (ui/action-bar bindings) -> nil
;   The runtime-owned Row 74 action strip. Scale-2, inverted. `bindings`
;   is a list of (key . label) pairs; each renders as `KEY label`, the
;   pairs ·-separated (micro/mc context-strip pattern). Bottom of the
;   surface: pixel row (height - 16), scale-2 = 16 px tall.
(defn ui/action-bar (bindings)
  (let y-px (- (render/height) 16))
  (ui/chrome-band y-px 16)
  (let parts (map (fn (b) (str (car b) " " (cdr b))) bindings))
  (let dot (str "  " (char->string UI-MIDDOT) " "))
  (render/text 0 y-px 2 UI-BLACK (join parts dot)))

; ===========================================================
;  headline — big-glyph title (§10.6)
; ===========================================================

; (ui/headline cx cy text scale) -> nil
;   A big-glyph headline at scale 4-5 (§5 type hierarchy) — the
;   "this is a screen / mission / event title" primitive. cx/cy are CELL
;   coords (converted to pixels for render/text). Drawn in the normal
;   phosphor pair (phosphor-on-black).
(defn ui/headline (cx cy text scale)
  (render/text (* cx 8) (* cy 8) scale UI-LIT text))

; ===========================================================
;  status-glyph — state icon (§10.4 / §4 identity channel)
; ===========================================================

; (ui/status-code state frame) -> glyph code
;   Map a state symbol to its §4 identity glyph. `frame` is the current
;   animation frame (the 20 fps tick); the `starting` state frame-cycles
;   between ◐ (UI-STARTING) and ○ (UI-STOPPED) on alternate frames to
;   read as "spinning up". All other states are steady.
(defn ui/status-code (state frame)
  (cond ((is state 'running)  UI-RUNNING)
        ((is state 'error)    UI-ERROR)
        ((is state 'stopped)  UI-STOPPED)
        ((is state 'starting) (if (even? frame) UI-STARTING UI-STOPPED))
        (else UI-STOPPED)))

; (ui/status-glyph col row state) -> nil
;   Render the state icon for `state` at cell (col,row), normal pair.
;   `running ● / starting ◐ / error ✖ / stopped ○`. Steady-frame variant;
;   pass frame 0. Use ui/status-glyph-anim for the frame-cycled form.
(defn ui/status-glyph (col row state)
  (render/glyph col row (ui/status-code state 0) 1 UI-LIT UI-BLACK))

; (ui/status-glyph-anim col row state frame) -> nil
;   Frame-cycled variant — `starting` animates against `frame` (§10.4).
(defn ui/status-glyph-anim (col row state frame)
  (render/glyph col row (ui/status-code state frame) 1 UI-LIT UI-BLACK))
