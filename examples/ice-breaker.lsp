; ice-breaker.lsp — Phase 1 DSL slice.
;
; A mission brief composed as a declarative screen tree (data), drawn by the
; ui/screen.lsp engine. Mixed scale on one screen: headline (5), subtitle
; (2), body (1), plus an absolute big clock via `at`. Mirrors the device
; photo (the "MIXED-SIZE DEMO" screen).

(ui/draw
 '(screen ()
    (status-bar (handle "ROOK" cr 2480))
    (stack (gap 1)
      (headline  (scale 5 align center) "ICE BREAKER")
      (subtitle  (scale 2 align center) "INTRUSION     STANDARD LEVEL")
      (rule ())
      (text () "The signal drops at 0300. Bring the cutter and do not")
      (text () "trip the ICE. Three gates, one timing window, no retries.")
      (spacer (h 1))
      (text () "TARGETS")
      (text () "1 SENTRY ICE    LV3")
      (text () "2 BLACK GATE    LV4")
      (text () "3 VAULT DOOR    LV5")
      (text () "4 TRACE DAEMON  LV2"))
    (at (x 744 y 200 scale 1) "WINDOW")
    (at (x 700 y 216 scale 4) "03:00")
    (action-bar () ("EVAL" "accept") ("CDR" "next") ("BACK" "exit"))))
