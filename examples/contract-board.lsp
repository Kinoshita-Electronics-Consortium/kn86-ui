; contract-board.lsp — Phase 1 DSL slice (mission list as flowed text).
(ui/draw
 '(screen ()
    (status-bar (handle "ROOK" cr 2480 sec "MISSIONS"))
    (stack (gap 1)
      (headline (scale 4 align center) "CONTRACT BOARD")
      (rule ())
      (text () "The signal drops at 0300. Bring the cutter and")
      (text () "do not trip the ICE. Reputation rides on this.")
      (spacer (h 1))
      (text () "1. ICE BREAKER      INTRUSION    STANDARD")
      (text () "2. BLACK LEDGER     FORENSIC      ADVANCED")
      (text () "3. DEPTHCHARGE      SONAR         ADVANCED"))
    (action-bar () ("EVAL" "accept") ("CDR" "next") ("BACK" "exit"))))
