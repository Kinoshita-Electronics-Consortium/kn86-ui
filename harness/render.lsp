; system-image/lib/render.lsp
;
; render/ — System-tier Fe wrapper library (GWP-509).
;
; The L3 `render/` library from the Fe-Lisp runtime architecture §5: thin
; KEC Lisp wrappers over the privileged render/* primitive set (§4b),
; loaded into System-tier contexts only. A cart context has no render/*
; symbols bound, so loading this library there raises "unbound symbol" —
; that is the capability boundary working as designed (capability is the
; binding-set; kec-lisp-language-standard §2.1/§5).
;
; Authoritative harness: src/sys_render.c binds the 10 render/*
; primitives (clear, text, glyph, fill-rect, box, bitmap, present, poll,
; width, height) into the context this file loads into
; (nosh_system_context_create, src/sys_context.c). These wrappers add the
; small ergonomics the ui/ kit (Phase 3) will reach for without re-deriving
; them each time.
;
; Convention: a `:single` weight is 0 / nil, `:double` weight is non-nil,
; matching render/box. Colour tokens follow sys_render.h — a lit token
; (non-nil / non-zero) paints the active phosphor hue, nil / 0 paints
; black (the inverted selection channel).
;
; KEC Core notes (mirrors carts/icebreaker.lsp + baselines/terminal.lsp):
;   - Forms available: defn / define / when (loaded by Core before libs).
;   - Render coords are pixels except render/glyph + render/box, which
;     take cell coordinates (the primitive multiplies cells by 8).

; ===========================================================
;  Frame lifecycle
; ===========================================================

; (render/begin-frame) -> nil
;   Clear the whole framebuffer to black. Call once at the top of a
;   System screen's render pass before drawing.
(defn render/begin-frame ()
  (render/clear))

; (render/end-frame) -> nil
;   Flush the composed surface to the host backend. Call once at the
;   bottom of a render pass.
(defn render/end-frame ()
  (render/present))

; ===========================================================
;  Text helpers
; ===========================================================

; (render/line x y text) -> nil
;   Draw a line of body text at scale 1 in the active phosphor hue.
(defn render/line (x y text)
  (render/text x y 1 1 text))

; (render/heading x y text) -> nil
;   Draw a scale-2 chrome / heading line in the active phosphor hue.
(defn render/heading (x y text)
  (render/text x y 2 1 text))

; (render/headline x y text) -> nil
;   Draw a scale-5 headline (the big-glyph attention channel) in the
;   active phosphor hue.
(defn render/headline (x y text)
  (render/text x y 5 1 text))

; ===========================================================
;  Frames
; ===========================================================

; (render/panel cx cy cw ch title) -> nil
;   Single-line bordered region (cell coords) with an optional title
;   cutout. Pass nil for an untitled panel.
(defn render/panel (cx cy cw ch title)
  (render/box cx cy cw ch nil title))

; (render/modal cx cy cw ch title) -> nil
;   Double-line bordered region (cell coords) — the focused-panel /
;   modal weight — with an optional title cutout.
(defn render/modal (cx cy cw ch title)
  (render/box cx cy cw ch 1 title))

; ===========================================================
;  Selection / fill
; ===========================================================

; (render/select-row x y w h) -> nil
;   Paint a solid phosphor-hue bar (px coords) — the inverted selection
;   span. Text drawn over it with an inverted fg reads as the §4
;   selection channel.
(defn render/select-row (x y w h)
  (render/fill-rect x y w h 1))

; (render/clear-rect x y w h) -> nil
;   Clear a pixel rectangle back to black (e.g. erase a popup region).
(defn render/clear-rect (x y w h)
  (render/fill-rect x y w h 0))

; ===========================================================
;  Surface query
; ===========================================================

; (render/center-x w-px) -> x
;   Left pixel x that centers a `w-px`-wide element on the surface.
(defn render/center-x (w-px)
  (/ (- (render/width) w-px) 2))

; (render/center-y h-px) -> y
;   Top pixel y that centers an `h-px`-tall element on the surface.
(defn render/center-y (h-px)
  (/ (- (render/height) h-px) 2))
