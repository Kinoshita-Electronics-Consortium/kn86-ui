; system-image/lib/ui/glyphs.lsp
;
; ui/ glyph + colour constants (GWP-510/511/512).
;
; ===========================================================
;  The ui/ component kit — placement + load contract
; ===========================================================
;
; The system-tier UI component kit (ui-design-language.md §10), mirroring
; the system-image/lib/render.lsp convention: thin KEC-Lisp components over
; the privileged render/* tier (sys_render.c). The kit loads into System-
; tier contexts only (nosh_system_context_create) — a cart context has no
; render/* symbols bound, so loading it there raises "unbound symbol",
; which is the capability boundary working as designed.
;
; Files + load order (dependency-correct — load render.lsp first, then ui/
; in this order; the kit has no `load` form of its own, so the host loads
; each file like render.lsp is loaded — see tests/ui_golden.h ui_lib_files):
;
;   render.lsp        the render/* wrapper lib (GWP-509) — loaded first;
;                     some ui/ helpers call render/center-x etc.
;   ui/glyphs.lsp     THIS FILE — glyph + colour constants, used by all
;                     other ui/ files; load before them.
;   ui/frames.lsp     cbox, panel/box, rule, status-bar, action-bar,
;                     headline, status-glyph (GWP-510).
;   ui/compositing.lsp scrim, shadow, select-row, modal, popup, palette
;                     (GWP-510). Depends on frames (ui/cbox, ui/rule).
;   ui/lists.lsp      list, tree, table (GWP-511). Depends on compositing
;                     (ui/select-row).
;   ui/data.lsp       sparkline, fine-bar (GWP-511). Depends on glyphs
;                     (ui/vbar, ui/hbar).
;   ui/input.lsp      tab-bar, field, form, slider, dial, cycler, toast,
;                     reveal/unreveal (GWP-512). Depends on frames +
;                     compositing.
;
; Authored in KEC Core vocabulary (defn / let / cond / when / dolist /
; dotimes / map / fold-left / floor / str / …), the same style as
; render.lsp + the carts. KEC uses `set` for assignment and `=` / `is` for
; equality (the kec-lisp migration); see vendor/kec-lisp/core/*.lsp.
;
; The KN-86 Code Page codes the ui/ component kit reaches for, named once
; so the component sources read as intent, not magic numbers. Codes are
; from the canonical character-set.md allocation table + the ADR-0036
; custom-glyph extension; cross-check against src/font.c.
;
; The four-channel rule (ui-design-language.md §4) is built out of these:
;   - identity  = a distinct glyph per kind (status icons, typed headers)
;   - magnitude = the density ramp (UI-SHADE-*) and the bar ramps
;     (UI-VBAR-*, UI-HBAR-*)
;   - selection = inversion (a black-on-phosphor colour pair), NOT a glyph
;   - attention = border weight + the leading-glyph column + blink
;
; Colour tokens follow sys_render.h: a lit token (non-nil / non-zero)
; paints the active phosphor hue; nil / 0 paints black. The inverted pair
; (fg black, bg lit) is the §4 selection channel.

; ----- Colour tokens (sys_render.h convention) ------------------------
(define UI-LIT   1)        ; foreground = active phosphor hue
(define UI-BLACK 0)        ; foreground = black

; ----- Box-drawing (character-set.md §box, font.c 0x80-0x95) -----------
; Single-line frame.
(define UI-BOX-H   128)    ; 0x80 ─
(define UI-BOX-V   129)    ; 0x81 │
(define UI-BOX-TL  130)    ; 0x82 ┌
(define UI-BOX-TR  131)    ; 0x83 ┐
(define UI-BOX-BL  132)    ; 0x84 └
(define UI-BOX-BR  133)    ; 0x85 ┘
; Double-line frame.
(define UI-BOX-DH  139)    ; 0x8B ═
(define UI-BOX-DV  140)    ; 0x8C ║
(define UI-BOX-DTL 141)    ; 0x8D ╔
(define UI-BOX-DTR 142)    ; 0x8E ╗
(define UI-BOX-DBL 143)    ; 0x8F ╚
(define UI-BOX-DBR 144)    ; 0x90 ╝

; ----- Status icons (§4 identity channel; font.c) ---------------------
(define UI-RUNNING  5)     ; 0x05 ● — running
(define UI-STOPPED  6)     ; 0x06 ○ — stopped
(define UI-STARTING 245)   ; 0xF5 ◐ — starting (frame-cycled)
(define UI-ERROR    246)   ; 0xF6 ✖ — error

; ----- Leading-glyph column (§4 attention channel) --------------------
(define UI-UNREAD   5)     ; 0x05 ● unread
(define UI-PINNED   42)    ; 0x2A * pinned
(define UI-BATCH    12)    ; 0x0C ► selected-for-batch / cursor
(define UI-SPACE    32)    ; 0x20 (normal)

; ----- Tree disclosure (§10.3) ----------------------------------------
(define UI-COLLAPSED 12)   ; 0x0C ► collapsed node
(define UI-EXPANDED  10)   ; 0x0A ▼ expanded node

; ----- Symbols --------------------------------------------------------
(define UI-LAMBDA   13)    ; 0x0D λ — callable marker (IntelliSense)
(define UI-SEP-ARROW 12)   ; 0x0C ► — status-bar field separator
(define UI-CURRENCY 14)    ; 0x0E ¤ — credit-balance prefix
; Action-bar binding separator. 0xFA (CP437 ·) is blank in this font, so
; the bullet ● (0x05) carries the "·"-separated read the spec describes
; (ui-design-language.md §10.1) with a glyph that actually renders.
(define UI-MIDDOT   5)      ; 0x05 ● — action-bar binding separator

; ----- Density ramp (§4 magnitude — area: ░▒▓█) -----------------------
; font.c: 0xA5 light, 0xA6 med, 0xA7 dark, 0xA2 full.
(define UI-SHADE-LIGHT 165) ; 0xA5 ░
(define UI-SHADE-MED   166) ; 0xA6 ▒ (the 0xAA/0x55 dither — the scrim cell)
(define UI-SHADE-DARK  167) ; 0xA7 ▓
(define UI-FULL        162) ; 0xA2 █

; ----- Vertical bar ramp (§4 magnitude — sparklines: ▁..█) -------------
; font.c 0x96..0x9D = 1/8..8/8 filled from the bottom.
(define UI-VBAR-0   32)     ; empty cell = space
(define UI-VBAR-1   150)    ; 0x96 ▁
(define UI-VBAR-8   157)    ; 0x9D █ (= full block; ramp top)

; (ui/vbar k) -> glyph code for k eighths (0..8). 0 -> space.
(defn ui/vbar (k)
  (cond ((<= k 0) UI-VBAR-0)
        ((<= 8 k) UI-VBAR-8)
        (else (+ 149 k))))   ; 0x96 is one-eighth -> 149 + k

; ----- Horizontal bar ramp (§4 magnitude — fine-bar: left-fill) -------
; font.c 0xA8..0xAE = 1/8..7/8 left-filled columns; 0xA2 = full.
(define UI-HBAR-0   32)     ; empty cell = space
(define UI-HBAR-7   174)    ; 0xAE 7/8

; (ui/hbar k) -> glyph code for k eighths of horizontal fill (0..8).
;   0 -> space, 8 -> full block, 1..7 -> the left-fill ramp 0xA8..0xAE.
(defn ui/hbar (k)
  (cond ((<= k 0) UI-HBAR-0)
        ((<= 8 k) UI-FULL)
        (else (+ 167 k))))   ; 0xA8 is one-eighth -> 167 + k
