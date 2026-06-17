---
title: UI Patterns
description: "Reusable UI patterns and audio-as-data conventions."
---

:::note
Extracted from [kn86-docs](https://kn86-deckline.com), the canonical engineering docs. ADR and cross-doc links resolve there.
:::


## Overview

This document defines the reusable UI component patterns, layout architectures, interaction paradigms, and visual conventions for all KN-86 Deckline cartridges. The design system ensures visual and functional consistency across the 14-cartridge library while allowing module-specific customization in appropriate layers.

**Platform Constraints:**

See CLAUDE.md Canonical Hardware Specification for the authoritative values (main display, auxiliary display, text grid, font, color, key layout, display modes). Summarized for convenience:

- Main display: 80-column × 25-row text grid (amber on black), owned jointly by firmware (rows 0 + 24) and cartridges (rows 1–23).
- Auxiliary display: CIPHER-LINE OLED above the keyboard, 4 logical rows, rendered by the nOSh runtime. Cipher voice appears here and ONLY here.
- Interaction: 31 physical keys (14 function + 16 numpad + 1 TERM, phone-layout per ADR-0016 / ADR-0022). See [`docs/software/runtime/input-dispatch.md`](../../runtime/input-dispatch.md).

**Orchestration Model:**
- The nOSh runtime owns: mission board, main-grid status bar (row 0), main-grid action bar (row 24), economy, phase chain, and the Cipher voice engine rendered onto CIPHER-LINE.
- Cartridges own: main-grid active phase content (rows 1–23), custom displays, sound design, and CIPHER-LINE grammar/vocabulary contributions (see `docs/software/runtime/cipher-voice.md`).
- Universal integration points: contract templates, deck state signatures, cross-module interactions, coherence stack persistence across cart swaps.

**Surface split (non-negotiable — Spec Hygiene Rule 6):** The main 80×25 grid never carries Cipher utterances. CIPHER-LINE is the sole output surface for the Cipher voice. Screen patterns in §3 apply to the main grid; CIPHER-LINE patterns are documented in §3A CIPHER-LINE below.

---

## 1. Screen Architecture Patterns

### Pattern 1: Full List Browser

**Purpose:** Contract selection, transaction lists, menu navigation, option selection.

**Layout (80×25 grid):**
```
Row 0   [STATUS BAR: operator handle, credits, reputation]
Row 1   [LIST TITLE: left-aligned]
Rows 2–22 [LIST CONTENT: one item per row]
        [> SELECTED_ITEM (current focus, prefixed with ">")]
        [ ] unselected_item]
        [ ] unselected_item]
Row 24  [ACTION BAR: CAR=enter CDR=next BACK=exit]
```

**Specifications:**
- **Title (row 1, cols 0–78):** Module-specific title, e.g., "CONTRACTS", "TRANSACTIONS", "TARGETS"
- **Content (rows 2–22, 21 visible items):** Each row is a single list item. Row structure:
  ```
  [>] ITEM_NAME                      [SECONDARY]
  [ ] ITEM_NAME                      [SECONDARY]
  ```
  - Col 0–1: Selection marker (">", " ")
  - Col 2–59: Item name (58 chars max, left-aligned, truncated if needed)
  - Col 60–78: Secondary data (right-aligned, 19 chars max: threat level, payout, status, etc.)

- **Focus:** Currently selected item highlighted with ">" prefix. Automatically positioned center-screen if scrolling.
- **Navigation:** CDR cycles items down; CONS (future) or QUOTE could bookmark items
- **Drill:** CAR enters selected item, passes control to detail inspector or phase handler

**Audio:** Soft key-click on CDR (YM2149 Voice 3, 1.2 kHz, 50ms envelope). Tone changes if jumping to/from list boundary.

**Scrolling Behavior:** If list has >21 items, display rows 2–22 are a window. Track a `view_offset` that recenters selected item when navigating past half-screen.

---

### Pattern 2: Split Pane (Dual List/Grid)

**Purpose:** Comparing two entity streams (Black Ledger's dual ledgers, ICE Breaker's node map + inventory).

**Layout (80×25 grid):**
```
Row 0   [STATUS BAR]
Row 1   [LEFT TITLE] | [RIGHT TITLE]
        (columns 0–38) | (columns 41–78)
Rows 2–22 [LEFT CONTENT] | [RIGHT CONTENT]
        [>] Item 1    | [>] Item A
        [ ] Item 2    | [ ] Item B
        ...           | ...
Row 24  [ACTION BAR: focus-aware hints]
```

**Specifications:**
- **Divider:** Column 39 is the split boundary. Cols 0–38 are left pane, cols 41–78 are right pane.
- **Each pane:** Operates independently as a list browser. Both have their own selection cursors.
- **Active pane:** Determined by most recent key press direction (CDR in left vs. CDR in right). CONS bridges panes (links selected items across boundary).
- **Titles (row 1):** Left title spans cols 0–38 (39 chars max); right title spans cols 41–78 (38 chars max).
- **Content rows (rows 2–22, 20 visible items per pane):** Same format as Pattern 1 but narrower:
  ```
  [>] NAME              [secondary]  |  [>] NAME              [secondary]
  ```
  - Left: cols 0–38 (39 chars per row)
  - Right: cols 41–78 (38 chars per row)

**Focus Indicator (row 24):** Shows which pane has focus. Example: "LEFT: CAR=enter CDR=next" or "RIGHT: CAR=open CDR=next CONS=link". Info key always applies to the focused pane's selection.

**Audio:**
- Navigation within pane: Same as Pattern 1
- Switching panes (if key binds support it): Rising tone (1.5 kHz, 100ms)
- CONS (link operation): Descending pair (1.2 kHz → 800 Hz, 150ms total)

---

### Pattern 3: Full Grid (Tile-Based View)

**Purpose:** Spatial layouts, network topology overviews, grid-based operations (NeonGrid's spatial grid).

**Layout (80×25 grid):**
```
Row 0   [STATUS BAR]
Row 1   [GRID TITLE: left-aligned]
Rows 2–23 [GRID CONTENT: 22 rows of grid cells]
        (Divide 80 columns into logical cell grid: 4×4, 5×5, 8×8 depending on cell size)
Row 24  [ACTION BAR: movement hints]
```

**Cell Size Examples:**
- **8×8 grid of 10×2-char cells** (80 cols / 10 = 8 columns; 22 rows / 2 = 11 rows): Good for high-level tactical overviews, node maps. Each cell: 10 chars wide, 2 rows tall.
  ```
  [ NODE_A ]  [ NODE_B ]  [ NODE_C ]  ...
  [   ***  ]  [   -+- ]  [   █░░ ]
  ```
- **10×11 grid of 8×2-char cells** (80 / 8 = 10 columns; 22 / 2 = 11 rows): Finer detail, inventory grids. Each cell: 8 chars wide, 2 rows tall.

**Cell Contents:**
- **Name/Label:** Row 0 of cell (top row, max 8–10 chars, centered or left-aligned)
- **Icon/Status:** Row 1 of cell (visual symbol: █, ░, -, +, *, !, etc.)

**Navigation:**
- CAR moves selection to current cell; context-dependent (enter node, inspect location, etc.)
- CDR cycles selection through adjacent cells (right → down → left → up wrapping)
- INFO shows details of selected cell
- ATOM tests if cell is "atomic" (leaf node, cannot drill deeper)

**Focus:** Current cell is highlighted with a **box border**:
```
┌─────────┐      instead of      [ NODE_A ]
│ NODE_A  │
│ ***     │
└─────────┘
```

**Audio:** Spatial "scan" tone that rises/falls with cursor position (e.g., pitch increases left→right, down→up).

---

### Pattern 4: Status Dashboard

**Purpose:** Real-time information display without drilling (operational status, mission progress, threat levels, system state).

**Layout (80×25 grid):**
```
Row 0   [STATUS BAR]
Rows 1–4 [SECTION 1: Key Metrics (rows 1–4, max 3 lines of content)]
        SECTION TITLE
        Metric A: ████░░ (50%)
        Metric B: ¤8,400

Rows 5–8 [SECTION 2: Secondary Metrics]
Rows 9–12 [SECTION 3: Alerts/Events]
Rows 13–23 [REMAINING: flexible per cartridge]

Row 24  [ACTION BAR: CDR=scroll CAR=detail BACK=exit]
```

**Specifications:**
- **Sections:** Logically divide rows 1–23 into 3–5 sections, each labeled with a bold section title (row N, cols 0–20).
- **Metrics:** Displayed with progress bars (─────) or simple text assignments.
- **Scrolling:** If content exceeds 22 rows, CDR scrolls sections down (one full section per press).
- **Detail drill:** CAR on a metric opens an inspector (Pattern 5).

**Section Examples:**
1. **MISSION STATUS:** Contract name, time elapsed, phase count, objective progress
2. **INVENTORY:** Equipment slots, ammo counts, capacity bars
3. **NETWORK STATE:** Alert level, ICE threat, system integrity
4. **ALERTS:** Recent events ("ICE_SPAWNED", "PAYOUT_CHANGED", etc.)

**Audio:** Periodic soft tones for metric updates (e.g., reputation increase = ascending tone; threat spike = alarm).

---

### Pattern 5: Detail Inspector (Modal/Focused View)

**Purpose:** Deep inspection of a single item, expanded details, confirmation dialogs, narrative content (mission briefings, technical logs). Cipher utterances render on CIPHER-LINE (§3A), not in this pattern.

**Layout (80×25 grid):**
```
Row 0   [STATUS BAR]
Row 1   [TITLE: centered or left-aligned, possibly in caps]
Row 2   [─────────────────────────────────────────────────────────────────────────────────]
Rows 3–22 [CONTENT: formatted text, attributes, nested lists, or narrative]
        Key: Value
        Key: Value
        Description paragraph (wrapped text, left-aligned)
        [Nested list:]
          > Item 1
          > Item 2
Row 23  [EMPTY or subtext]
Row 24  [ACTION BAR: CAR=confirm/accept BACK=cancel QUOTE=bookmark]
```

**Specifications:**
- **Full width (cols 0–79):** No left/right panes; content dominates.
- **Title (row 1):** Contract name, item name, or section header (centered or left-aligned, 78 chars max).
- **Divider (row 2):** Optional horizontal line (all dashes: `────...`).
- **Content (rows 3–22, 20 rows):** Flexible; typically:
  - Key-value pairs: `KEY_NAME..................value` (right-aligned value at col 60+)
  - Formatted text blocks (wrapped, left-aligned)
  - Nested lists with indentation (2 spaces per level)
- **Subtext (row 23):** Optional fine print (grayed out conceptually, but still amber on black).

**Examples:**
```
Row 1: CONTRACT: INFILTRATE SIGMA NODE
Row 2: ────────────────────────────────────────────────────────────────────────────────
Row 3: Threat Level.............................4/5 (RED ICE)
Row 4: Payout...................................¤15,000
Row 5: Time Limit...............................8:00
Row 6: Phases...................................2
Row 7: 
Row 8: OBJECTIVE:
Row 9: Retrieve encrypted payload from node 7. Exit node cleanly.
Row 10: 
Row 11: RESTRICTIONS:
Row 12:  > 2 alerts permitted (3+ triggers full network collapse)
Row 13:  > No destructive attacks (target data must remain intact)
Row 14: 
Row 15: REWARD SCALING:
Row 16: Perfect (0 alerts).......................¤18,000 + rep
Row 17: Clean (1–2 alerts)........................¤15,000
Row 18: Compromised (3+ alerts)...................¤8,000
Row 19:
Row 23: [CDR for more | EVAL to accept]
Row 24: [CAR=accept BACK=cancel QUOTE=defer]
```

**Scrolling:** If content >20 rows, CDR scrolls up/down (3 rows per press).

**Audio:** Upon enter: Single ascending tone (1.2 kHz, 100ms). Upon close: Descending tone (800 Hz, 100ms).

---

### Pattern 6: Combat/Action Grid (Real-Time)

**Purpose:** Active engagement: ICE Breaker node combat, tactical grid interactions, time-pressured decisions.

**Layout (80×25 grid):**
```
Row 0   [STATUS BAR: time remaining, threat level, current tool/action]
Row 1   [ACTION LABEL: "ICE COMBAT", "NETWORK NODE", etc.]
Rows 2–18 [COMBAT GRID: 4×4 node grid OR damage/status indicators]
Row 19  [TIME REMAINING: ████░░ 2:34]
Row 20  [THREAT LEVEL: ███░░░ 4/5 (escalating)]
Row 21  [INVENTORY/ACTIVE TOOL: SLICER loaded]
Row 22  [EMPTY]
Row 24  [ACTION BAR: CAR=attack CDR=move APPLY=use-tool EVAL=special BACK=retreat]
```

**Specifications:**
- **Combat Grid (rows 2–18, 17 rows):** Divide into combat arena. Examples:
  - **Node combat:** 4×4 grid of node states (open/defended/locked)
  - **Damage track:** Visual bars showing adversary health, player health, system integrity
  - **Action menu:** 3×3 grid of available tactical options
- **Time Bar (row 19):** `TIME ████░░░░ 2:34` (visual bar on left, MM:SS on right)
- **Threat Bar (row 20):** `THREAT ███░░░ 4/5` (escalation visualized)
- **Inventory Line (row 21):** Current equipped tool, ammo count, or special ability status

**Real-Time Mechanics:**
- Time ticks down continuously (YM2149 clicks per tick)
- Every keypress triggers an action immediately (no menu navigation)
- CAR = attack; EVAL = special power; APPLY = use item; BACK = retreat (costs time + risk)

**Audio:**
- **Threat escalation:** Rising arpeggiated tones (three notes, 1.0 → 1.2 → 1.4 kHz) as threat bar fills
- **Action execution:** Different tones per action type (attack = sharp click; special = sweep; retreat = lower tone)
- **Damage taken:** Brief burst of noise (YM2149 noise channel)
- **Time warning:** When <30 seconds remain, accelerating ticks (100ms → 50ms → 20ms intervals)

---

## 2. Navigation Grammar

### Core Navigation Intent Matrix

The Lisp-inspired key semantics must operate consistently across all screen patterns:

| Key | Pattern 1: List | Pattern 2: Split Pane | Pattern 3: Grid | Pattern 4: Dashboard | Pattern 5: Inspector | Pattern 6: Combat |
|-----|-----------------|----------------------|-----------------|----------------------|----------------------|-------------------|
| **CAR** | Drill into selected item | Drill into focused pane's selected item | Move to cell, trigger action | Open detail for selected metric | Accept/confirm | Attack / Execute |
| **CDR** | Cycle down (next item) | Cycle down in focused pane | Cycle adjacent cell (clockwise) | Scroll sections down | Scroll content down | Move cursor / Cycle targets |
| **CONS** | Combine two items (if applicable) | Link left & right selections across pane boundary | Combine/attach cell to current focus | N/A | N/A | Combine effects / Multi-action |
| **NIL** | Clear / deselect | Clear focus | Reset / center grid | N/A | N/A | Cancel action / Clear effect |
| **EVAL** | Execute action on selected item | Execute action in focused pane | Execute action on cell | N/A | Confirm decision | Deploy special ability / Execute |
| **QUOTE** | Bookmark / flag for later | Bookmark both selections | Bookmark cell | Bookmark metric | Bookmark detail | Mark target for follow-up |
| **APPLY** | Use tool on selected item | Use tool in focused pane | Apply tool to cell | N/A | N/A | Use equipped tool / Item |
| **ATOM** | Test if item is atomic | Test if selection is leaf | Test if cell is terminal (no sub-cells) | N/A | N/A | N/A |
| **EQ** | Compare with quoted item | Compare left vs right (implicit) | Compare quoted cell with current | N/A | N/A | N/A |
| **BACK** | Return to parent / exit | Exit to previous menu | Pan out / return | Close inspector | Cancel / back | Retreat (costs time + risk) |
| **INFO** | Summary of selected item | Summary of focused pane's item | Summary of cell | Expand section | Full detail view | Scan / diagnostics |
| **LINK** | Transmit / save selection | Link selections (explicit network egress) | Link node to external | N/A | Transmit detail | N/A |

### Navigation Rules by Context

#### Rule 1: List Views (Pattern 1)

1. **Initial Entry:** List displays. Selection cursor defaults to first item (row 2).
2. **CDR Navigation:** Moves cursor down one row; wraps to top if at bottom.
3. **CAR (Drill):** Opens Pattern 5 (inspector) or triggers phase handler for current item.
4. **BACK:** Returns to parent context (mission board, inventory hub, etc.).
5. **INFO (single-tap):** Shows summary line in action bar or opens abbreviated inspector.
6. **INFO (double-tap):** Opens full Pattern 5 detail inspector.
7. **QUOTE:** Bookmarks the selected item. Visual: row 2 shows `[◊]` instead of `[>]`.

**Example: Mission Board List**
```
Rows 2–22: Contract listings
Row 2 (selected): [>] INFILTRATE SIGMA NODE           T4 ¤15,000
Row 3: [ ] FORENSIC DIVE: CIPHER CORP                 T2 ¤8,000
CDR → Cursor moves to row 3; row 2 becomes unselected
CAR → Open inspector (Pattern 5) for INFILTRATE SIGMA NODE
QUOTE → Row 2 becomes [◊] INFILTRATE SIGMA NODE (bookmarked)
BACK → Return to boot screen or hub menu
```

#### Rule 2: Split Pane Views (Pattern 2)

1. **Pane Focus:** Initially, left pane is focused (default). Most recent navigational key in left vs. right determines focus.
2. **CDR:** Moves selection in current focused pane only.
3. **CAR:** Drills into selected item in focused pane.
4. **CONS:** Links selected items across both panes. Creates a connection record or flags a relationship.
5. **Switching Panes:** User must explicitly press CDR in the non-focused pane to redirect focus. (Alternative design: EQ key switches panes, but CDR-as-implicit-switch is less clear.)
6. **BACK:** Returns to previous context (mission board, etc.).
7. **INFO:** Shows detail for current pane's selection.

**Example: Black Ledger Dual Ledger**
```
Pane 1 (LEFT): Account A transaction list
Pane 2 (RIGHT): Account B transaction list

Initial state: Left pane focused
Row 2, Left [>] Transfer: 50K ¤      Row 2, Right [ ] Wire: 45K ¤
Row 3, Left [ ] Deposit: 10K ¤       Row 3, Right [>] Refund: 5K ¤

CDR → Left pane moves to row 3; left still focused
CDR → Left pane wraps to row 2 (cyclic)
CONS → Link selected items: left's "Transfer 50K" <→ right's "Refund 5K"
       (Creates a "possible match" or "flagged correlation")
```

#### Rule 3: Grid Views (Pattern 3)

1. **Initial Entry:** Cursor centered on grid (e.g., center of 8×8 node grid = node [4,4]).
2. **CDR:** Moves cursor in a clockwise direction (right → down → left → up → right...).
3. **Alternative:** Some grids allow cardinal movement (up/down/left/right). If so, specify in cartridge docs. Otherwise, CDR spirals clockwise.
4. **CAR:** Activates current cell. Drill into node, inspect cell state, or trigger action.
5. **ATOM:** Returns "true" if cell is a leaf node (no sub-nodes, cannot drill deeper).
6. **INFO:** Shows cell details (strength, type, distance to objective, etc.).
7. **BACK:** Pans out one level (e.g., from node detail view to full network overview).
8. **QUOTE:** Bookmarks cell for route planning (ICE Breaker: marks safe nodes to path through).

**Example: ICE Breaker Network Map**
```
8×8 Node Grid (rows 2–18, cols 0–79):
Row 2:   [░] [░] [░] [█] [!] [░] [░] [░]
Row 4:   [░] [█] [█] [█] [█] [░] [░] [░]
Row 6:   [█] [█] [S] [░] [░] [░] [█] [█]
Row 8:   [░] [░] [░] [░] [!] [X] [░] [░]

[S] = start (player), [X] = exit, [!] = ICE, [█] = firewall, [░] = empty

CDR → Cursor moves right (wraps after rightmost column to leftmost of next row)
CAR → Enter current node; open node combat (Pattern 6) or detail view
QUOTE → Mark node as "safe" (visual: change [░] to [◇])
BACK → Return to contract board
```

#### Rule 4: Dashboard Views (Pattern 4)

1. **Initial Entry:** Cursor positioned on first section.
2. **CDR:** Scrolls down (entire section moves). Cursor remains in relative position.
3. **CAR:** Opens inspector (Pattern 5) for the currently-focused metric or section.
4. **BACK:** Returns to previous menu or mission board.
5. **INFO:** Expands current metric with details (e.g., INFO on threat bar shows "ICE: 2× BLACK, 1× RED HUNTER").

**Example: Operational Dashboard**
```
ACTIVE MISSION: INFILTRATE SIGMA NODE
────────────────────────────────────────
Time Elapsed: ██████░░░ 4:26
Threat Level: ████░░░░░ 3/5
Phase: 1/2
Objective: Retrieve data

INVENTORY
────────────────────────────────────────
Slicer: ████░░░░░ (6/8 charge)
Cracker: ░░░░░░░░░░ (0/8 — depleted)
Worm: ██░░░░░░░░ (2/8)

ALERTS (Recent)
────────────────────────────────────────
[!] ICE updated to level 2 (node 5)
[!] HUNTER spawned (node 3 vicinity)

CDR → Scroll down to next section (INVENTORY, then ALERTS, etc.)
CAR on "Slicer: ..." → Open inspector showing tool details, efficacy, upgrade path
BACK → Return to mission board
```

#### Rule 5: Inspector Views (Pattern 5)

1. **Initial Entry:** Inspector displays title and content. Cursor at top.
2. **CDR:** Scrolls content down (3–5 rows per press).
3. **CAR:** Confirms/accepts the inspected item (e.g., "bid on contract", "equip tool").
4. **BACK:** Cancels and returns to parent context without changes.
5. **QUOTE:** Bookmarks the item for later reference (does not accept/confirm).
6. **INFO:** Toggles detailed view (if applicable; e.g., toggle between "summary" and "expanded stats").

**Example: Contract Inspector**
```
CAR to inspect a contract from mission board

CONTRACT: INFILTRATE SIGMA NODE
────────────────────────────────────────
Threat Level.............................4/5 (RED ICE)
Payout...................................¤15,000
Time Limit...............................8:00
Phases...................................2

OBJECTIVE:
Retrieve encrypted payload from node 7.

RESTRICTIONS:
 > 2 alerts permitted
 > No destructive attacks

CDR → Scroll down to see REWARD SCALING section
CAR → Accept contract (return to mission board, auto-enter phase 1)
BACK → Cancel; return to contract list without accepting
QUOTE → Bookmark contract for later (no action taken)
```

#### Rule 6: Combat/Action Views (Pattern 6)

1. **Initial Entry:** Combat grid displays. Cursor at center or current target.
2. **CDR:** Moves targeting cursor to next adjacent cell (clockwise or to next enemy).
3. **CAR:** Execute attack/action on current target.
4. **EVAL:** Deploy special ability (varies per cartridge: ICE Breaker's EXPLOIT, etc.).
5. **APPLY:** Use equipped tool/consumable item.
6. **BACK:** Retreat (costs time and increases alert/threat).
7. **No text scrolling:** All navigation is positional (grid movement, not linear).

**Example: ICE Breaker Node Combat**
```
Rows 2–18: Combat encounter (4 adjacent ICE nodes)
     [JUNK]    [BLACK]    [JUNK]    [─────]
      ↓          ↓          ↓

CDR → Cycle targeting between: JUNK (top-left) → BLACK (top-right) → JUNK (bottom-left) → warp back to JUNK (top-left)
CAR → Attack current target with equipped tool (Slicer? Cracker?)
EVAL → Use special ability (if unlocked; e.g., "REBOOT NODE")
APPLY → Use consumable (antivirus, etc.)
BACK → Retreat from node (costs 1 TIME + triggers ALERT)
```

---

## 3. Standard HUD Components

### 3.1 Status Bar (Row 0)

**Width:** 80 characters (cols 0–79)
**Height:** 1 row
**Content:** Left-aligned operator handle, center time/phase, right-aligned credits/reputation/threat

**Default Format:**
```
OPERATOR_HANDLE ¤8,400 (5,000/day)  [PHASE 1/2] TIME 2:34  REP 12  THR █████░░░░
```

**Field Breakdown (left to right):**
1. **Operator Handle (cols 0–14, 15 chars):** e.g., "NEXUS" padded to 15 chars with spaces
2. **Credits (cols 15–30, 15 chars):** `¤8,400 (5,000/day)` or `¤8,400` (format varies by context)
3. **Phase Info (cols 31–42, 12 chars):** `[PHASE 1/2]` or empty if not in mission
4. **Time (cols 43–52, 10 chars):** `TIME 2:34` or `STANDBY` if idle
5. **Reputation (cols 53–61, 9 chars):** `REP 12` or empty
6. **Threat Bar (cols 62–79, 18 chars):** `THR █████░░░░ 4/5` (visual bar + numeric)

**Customization Points:**
- Cartridges can add a module-specific status indicator (e.g., ICE Breaker's ALERT level) in place of phase info
- Threat bar can be replaced with health bar, system integrity, or network status

**Example: ICE Breaker Status Bar**
```
NEXUS          ¤15,000 (8,000/day)  [ALERT 2/3]  TIME 2:34  REP 12  ICE ████░░░░
```

**Example: Black Ledger Status Bar**
```
FORENSIC       ¤32,100               [PHASE 1/1]  TIME 5:20  REP 24  SUS ██████░░
```

---

### 3.2 Action Bar (Row 24)

**Width:** 80 characters
**Height:** 1 row
**Content:** Context-sensitive key hints

**Format:** `KEY_NAME=action_description KEY_NAME=action_description ...`

**Default Key Assignments (Pattern 1: List Browser):**
```
CAR=enter CDR=next BACK=exit INFO=detail QUOTE=mark CONS=link EVAL=accept
```

**Space Management (80 chars):**
- Each key hint is ~12 chars: `CAR=enter ` (10 chars) + space
- Maximum ~6–7 key hints fit in 80 chars before overflow
- Prioritize: CAR, CDR, BACK, EVAL, QUOTE, INFO, APPLY (in order of frequency)
- Secondary/rare keys (CONS, NIL, ATOM, EQ) only shown if relevant

**Context Examples:**

**List Browser (Pattern 1):**
```
CAR=enter CDR=next BACK=exit INFO=detail QUOTE=mark EVAL=accept
```

**Split Pane (Pattern 2):**
```
CAR=open CDR=next CONS=link BACK=exit LEFT/RIGHT=focus
```

**Grid View (Pattern 3):**
```
CAR=action CDR=move QUOTE=mark INFO=scan BACK=pan EVAL=special
```

**Combat (Pattern 6):**
```
CAR=attack CDR=move EVAL=ability APPLY=tool BACK=retreat
```

**Inspector (Pattern 5):**
```
CAR=accept BACK=cancel QUOTE=mark CDR=scroll
```

**Audio Association:** When user presses a key, a unique tone briefly plays to confirm the key was registered:
- Low tone (800 Hz): Navigation (CDR, cursor movement)
- Mid tone (1.2 kHz): Drill/confirm (CAR, EVAL)
- High tone (1.5 kHz): Special (APPLY, CONS)

---

### 3.3 Progress Bars

**Format Options:**

**Option A: Compact Bar**
```
Metric: ████░░░░░░ (50%)
```
- 10-character bar (█ = filled, ░ = empty)
- Value as percentage in parentheses
- Total width: ~35 characters

**Option B: Labeled Bar**
```
Health: ███████░░░ 70/100
```
- Bar + current/max values

**Option C: Minimal (for tight spaces)**
```
████░░░░░░
```
- Bar only, no label (label on previous row)

**Usage Examples:**

**Threat Bar (5-level escalation):**
```
THREAT ████░░░░░░ 3/5
THREAT ██████░░░░ 4/5  ← escalating
THREAT ██████████ 5/5  ← critical (all filled)
```

**Time Remaining:**
```
TIME ███░░░░░░░░ 4:26
```

**Health/Integrity:**
```
SLICER  ██████░░░░ 6/8 charge
HEALTH  ████████░░ 80/100 HP
```

**Completion:**
```
MISSION ███████░░░ 70% complete
```

**Visual Characters:**
- Filled: █ (U+2588, full block)
- Empty: ░ (U+2591, light shade)
- Half-filled (if needed): ▌ (U+258C, left half block)

**Calculation:** `bar_length = (current / max) * 10`, round down. Display that many █ followed by remaining ░.

---

### 3.4 Threat Indicators & Status Symbols

**Threat Levels (1–5 scale):**
```
1 = [░░░░░░░░░░] GREEN   (safe)
2 = [██░░░░░░░░] YELLOW  (caution)
3 = [████░░░░░░] ORANGE  (alert)
4 = [██████░░░░] RED     (danger)
5 = [██████████] CRITICAL (system failure imminent)
```

**Status Indicators (inline symbols):**
```
[■] Loaded / Ready / Active
[□] Empty / Unloaded / Standby
[◈] Processing / In Progress
[◊] Bookmarked / Flagged
[!] Alert / Warning
[×] Error / Failed / Locked
[+] New / Added / Bonus
[*] Special / Notable
[√] Complete / Success / Verified
```

**System Status (row 24 or dashboard):**
```
[■] NETWORK ACTIVE
[□] NETWORK OFFLINE
[×] NETWORK FAILED
[!] ALERT: Level 2
[√] OBJECTIVE COMPLETE
```

**ICE Threat Classification (in grid cells or list items):**
```
[J] = JUNK ICE (low threat)
[B] = BLACK ICE (medium threat)
[R] = RED ICE (high threat)
[H] = HUNTER ICE (pursuing threat)
```

**Firewall/Defense States (grid cells):**
```
[─] = Open node
[+] = Defended (ICE present)
[█] = Locked / Firewall engaged
[*] = Special node (objective, exit, etc.)
```

---

### 3.5 Pane Dividers & Separators

**Horizontal Divider (full width, row separator):**
```
────────────────────────────────────────────────────────────────────────────────
```
(80 dashes)

**Section Divider (lighter):**
```
————————————————————————————————————————————————————————————────────────────────
```
(80 em-dashes, or same as above depending on available charset)

**Vertical Divider (between panes, column 39–41):**
```
Column 39: │ (pipe character)
Column 40: (space)
Column 41: (space)
```

Or more prominently:
```
Column 39: ║ (double-line pipe, if charset supports)
```

**Example: Split Pane with Dividers**
```
Rows 2–22:
[>] Item 1        status │ [>] Item A        status
[ ] Item 2        status │ [ ] Item B        status
[ ] Item 3        status │ [ ] Item C        status
```

---

### 3.6 Tab Strips

**Purpose:** Switch between multiple views within the same context (e.g., mission board's 5 tabs, NeonGrid's grid/map/tactical views).

**Format (Row 1, cols 0–78):**
```
[ TAB_1 ] [ TAB_2 ] [ TAB_3 ] [ TAB_4 ] [ TAB_5 ]
```

or active tab highlighted:

```
[*TAB_1 ] [ TAB_2 ] [ TAB_3 ] [ TAB_4 ] [ TAB_5 ]
```

**Width per Tab:**
- ~15 characters per tab for 5 tabs = 75 chars used; 5 chars margin
- Adjust based on number of tabs

**Example (Mission Board):**
```
[ ACTIVE ] [ ARCHIVE ] [ FAILED ] [ PENDING ] [ STATS ]
```

**Active Tab Indicator:**
- Prefix with `*`: `[*ACTIVE ]` or
- Use box: `┌───────┐` instead of `[       ]` for active tab
- Color (conceptually amber highlight, rendered as bright text): not possible in monochrome, so rely on prefix/border

**Navigation (Tab Switch):**
- **Method 1:** EQ key cycles tabs (left → right → left)
- **Method 2:** Info key toggles tab menu above the content
- **Method 3:** Dedicated SYS-key sub-menu (hold SYS, press numbered key)

**Interaction:**
1. User presses EQ (or designated tab-switch key)
2. Cursor/focus moves to next tab
3. Content area (rows 2–23) repopulates with new tab's data
4. Audio: Soft "tab switch" tone (1.0 kHz, 100ms)

---

### 3.7 Data Tables

**Purpose:** Display lists of structured data (transactions, contracts, equipment, logs) with aligned columns.

**Format (Rows 2–22, 21 rows max visible):**
```
[>] COL1               COL2         COL3           COL4
[ ] Value1             Data1        Status         Amount
[ ] Value2             Data2        Status         Amount
```

**Column Alignment Rules:**
- **Column 1 (cols 0–1):** Selection marker `[>]` or `[ ]`
- **Column 2 (cols 2–39):** Primary data (left-aligned, 38 chars max)
- **Column 3 (cols 40–59):** Secondary data (left-aligned, 20 chars)
- **Column 4 (cols 60–79):** Tertiary/numeric (right-aligned, 20 chars)

**Example: Transaction Table (Black Ledger)**
```
ROW 0: Account ALPHA Transaction History       Date        Amount
ROW 1: ──────────────────────────────────────────────────────────
ROW 2: [>] Wire Transfer: DELTA SYSTEMS        2026-04-10  ¤50,000
ROW 3: [ ] Deposit: Unknown source             2026-04-08  ¤25,000
ROW 4: [ ] Withdrawal: Sigma Corp              2026-04-05  ¤15,000
ROW 5: [ ] Interest payment                    2026-04-01  ¤1,200
```

**Example: Equipment Table (Inventory)**
```
ROW 0: INVENTORY              Charge   Installed  Cost
ROW 1: ─────────────────────────────────────────────────
ROW 2: [>] Slicer (v3)        6/8      Y          ¤4,000
ROW 3: [ ] Cracker (v2)       0/8      N          ¤6,000
ROW 4: [ ] Worm (v1)          2/8      Y          ¤2,000
ROW 5: [ ] Firewall Walker    8/8      Y          ¤8,000
```

**Headers:**
- If needed, display column headers in row 1 (after pattern 1's title row)
- Use bold (conceptually) or spacing to distinguish headers from data
- Align headers with columns below

---

### 3.8 ASCII Graphs & Maps

**Node Map (for network topology, ICE Breaker, Nodespace):**

**Vertical Tree (text flow, top to bottom):**
```
                        [ROOT]
                          |
                    [GATEWAY] – [ROUTER]
                     /       \
                [NODE1]    [NODE2]
                  / \         |
              [N1A] [N1B]  [N2A]
```

**Horizontal Graph (text flow, left to right):**
```
[START] ──> [NODE1] ──> [NODE2] ──> [EXIT]
              |            |
             [NODE1a]    [NODE2a]
```

**Grid-Based Map (spatial 2D, for NeonGrid):**
```
  0   1   2   3   4
0 [░] [░] [+] [░] [░]
1 [░] [█] [█] [█] [░]
2 [+] [█] [S] [█] [+]
3 [░] [█] [█] [█] [░]
4 [░] [░] [X] [░] [░]

[S] = start   [X] = objective   [+] = waypoint   [░] = empty   [█] = wall
```

**Sonar/Heatmap (threat proximity, for ICE Breaker):**
```
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░▒▒▒▒▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒▒▒▒▒░░░░░░░░░░░░░░░░░░░░
░░░░░░▒▒███▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒███▒▒░░░░░░░░░░░░░░░░░░░░░░
░░░░░▒▒███▓███▒▒░░░░░░░░░░░░░░░░░░░░░░░░░▒▒███▓███▒▒░░░░░░░░░░░░░░░░░░░░░
░░░░░▒████▓▓▓████▒░░░░░░░░░░░░░░░░░░░░░▒████▓▓▓████▒░░░░░░░░░░░░░░░░░░░░░

░ = cool (safe)   ▒ = warm (caution)   █ = hot (threat)   ▓ = critical (danger)
```

**ASCII Art Constraints:**
- Use only ASCII characters (0–127) or extended box-drawing (see section 3.9)
- Maximum width: 78 characters (leave cols 0–1 for selection marker if needed)
- Represent dense data efficiently (each character is valuable)
- Consider bitmap mode (Pattern 3 fallback) if ASCII becomes too cramped

---

### 3.9 Box Drawing & Borders

**Single-Line Box:**
```
┌────────────────────────────────────────┐
│ CONTENT AREA                           │
│                                        │
└────────────────────────────────────────┘
```

**Double-Line Box (for emphasis):**
```
╔════════════════════════════════════════╗
║ CRITICAL ALERT                         ║
║                                        ║
╚════════════════════════════════════════╝
```

**Partial Box (top & bottom only):**
```
┌────────────────────────────────────────┐
 SECTION TITLE
 Content line 1
 Content line 2
└────────────────────────────────────────┘
```

**Box-Drawing Characters:**
```
┌ = top-left corner
┐ = top-right corner
└ = bottom-left corner
┘ = bottom-right corner
─ = horizontal line
│ = vertical line
├ = left tee
┤ = right tee
┬ = top tee
┴ = bottom tee
┼ = cross

╔ = double top-left
╗ = double top-right
╚ = double bottom-left
╝ = double bottom-right
═ = double horizontal
║ = double vertical
```

**Example: Modal Alert**
```
╔════════════════════════════════════════════════════════════════════════════════╗
║ [!] NETWORK COMPROMISED                                                        ║
║────────────────────────────────────────────────────────────────────────────────║
║ Your intrusion signature has been detected by ICE HUNTER.                       ║
║ Threat level escalated to 5/5.                                                 ║
║                                                                                 ║
║ RECOMMEND: Retreat immediately or engage evasion protocol.                     ║
║────────────────────────────────────────────────────────────────────────────────║
║ [EVAL to continue] [BACK to abort]                                             ║
╚════════════════════════════════════════════════════════════════════════════════╝
```

---

### 3.10 Modal Overlays & Dialogs

**Purpose:** Confirmation prompts, critical alerts, hot swap requests, runtime-level dialogs that demand the operator's attention on the main grid.

**Scope note (2026-04-24):** Earlier revisions of this section listed "Cipher voice announcements" as a modal use case. That is superseded. Cipher utterances appear only on CIPHER-LINE (§3A CIPHER-LINE; Spec Hygiene Rule 6). Modals are for runtime-level interactive prompts that need a main-grid response — they are not Cipher's surface.

**Layout:**
```
Rows 0–23: Faded/unchanged (background)
Rows 7–17: Modal box (11 rows, centered vertically, overlaid)
           Cols 2–77: Modal content (76 chars wide, centered horizontally)
```

**Format:**
```
╔════════════════════════════════════════════════════════════════════════════════╗
║ [ICON] TITLE                                                                   ║
║────────────────────────────────────────────────────────────────────────────────║
║                                                                                 ║
║  Message text, wrapped to ~70 characters per line. May span multiple lines.     ║
║  nOSh-runtime-level interactive prompts only.                                  ║
║                                                                                 ║
║────────────────────────────────────────────────────────────────────────────────║
║                    [EVAL=continue] [BACK=cancel]                               ║
║                                                                                 ║
╚════════════════════════════════════════════════════════════════════════════════╝
```

**Icon Options:**
- `[!]` = Alert / Warning
- `[*]` = Announcement / Info
- `[√]` = Success / Confirmation
- `[×]` = Error / Failure
- `[?]` = Query / Question
- `[◈]` = Processing / Loading

The `[▶]` "Cipher Voice" icon that appeared in earlier revisions is retired on the main grid. Cipher commentary renders on CIPHER-LINE (§3A CIPHER-LINE) and has its own visual conventions there.

**Example: Confirmation Prompt**
```
╔════════════════════════════════════════════════════════════════════════════════╗
║ [?] BID ON CONTRACT?                                                           ║
║────────────────────────────────────────────────────────────────────────────────║
║                                                                                 ║
║  INFILTRATE SIGMA NODE — ¤15,000 payout, 8-minute time limit, threat 4/5.     ║
║  Accept this contract? Once accepted, you cannot back out without penalty.      ║
║                                                                                 ║
║────────────────────────────────────────────────────────────────────────────────║
║                   [EVAL=accept] [BACK=decline]                                 ║
║                                                                                 ║
╚════════════════════════════════════════════════════════════════════════════════╝
```

**Example: Hot Swap Request (Multi-Phase Mission)**
```
╔════════════════════════════════════════════════════════════════════════════════╗
║ [*] PHASE 2/3: REQUIRES SIGNAL MODULE                                          ║
║────────────────────────────────────────────────────────────────────────────────║
║                                                                                 ║
║  Current phase requires the SIGNAL cartridge. Your options:                     ║
║   1. Swap cartridge (manual: physically replace module)                         ║
║   2. Select from library (simulated: choose from installed cartridges)          ║
║   3. Defer mission (put this contract on hold until you have the module)        ║
║                                                                                 ║
║────────────────────────────────────────────────────────────────────────────────║
║              [CAR=select] [QUOTE=defer] [BACK=abort mission]                   ║
║                                                                                 ║
╚════════════════════════════════════════════════════════════════════════════════╝
```

**Audio on Modal:**
- Open modal: Ascending tone (1.2 kHz, 150ms) + brief silence
- User response (CAR): Confirmation tone (1.5 kHz, 100ms)
- Close modal: Descending tone (900 Hz, 100ms)

---

### 3.11 Scrolling Text Areas

**Purpose:** Display long-form main-grid content — mission briefings, technical logs, cartridge-authored narrative text. Cipher utterances are not routed through this pattern; they render on CIPHER-LINE (§3A CIPHER-LINE).

**Format (Rows 2–22, 21 rows visible):**
```
Row 1: [TEXT AREA TITLE]
Row 2: ────────────────────────────────────────────────────────────────────────────────
Rows 3–22: [Text content, left-aligned, wrapped]
Row 23: [Scrolling indicator: "▼ 3 more lines" or "↑ 5 lines above"]
Row 24: [ACTION BAR: CDR=scroll BACK=exit QUOTE=save]
```

**Text Wrapping:**
- Width: 78 characters per line
- If a word exceeds 78 chars, break mid-word and hyphenate (rare; use short words)
- Blank lines between paragraphs (double newline → single blank row)

**Scrolling Behavior:**
- CDR scrolls down 5 rows per press (or to end if fewer than 5 rows remain)
- CONS scrolls up 5 rows (or to top)
- Row 23 shows scrolling status:
  - Top of text: `[Start of message]`
  - Middle: `▼ 8 more lines` (lines remaining below)
  - Bottom: `[End of message]`

**Example: Mission Briefing (nOSh-runtime-authored, main grid)**
```
Row 1: MISSION BRIEFING: INFILTRATE SIGMA NODE
Row 2: ────────────────────────────────────────────────────────────────────────────────
Row 3: Your target is SIGMA NODE, a secondary command hub for Delta Systems. 
Row 4: Intelligence suggests the encryption keys are stored in partition 7.
Row 5: 
Row 6: The network has moderate ICE presence: BLACK class on the gateway, with
Row 7: JUNK scattered through secondary nodes. Time limit is 8 minutes. The HUNTER
Row 8: algorithm activates after 3 visits to the same node—plan your routing.
Row 9: 
Row 10: You're cleared for non-destructive infiltration. The data must remain intact.
Row 11: Alerts escalate threat level. Reach 3 alerts, the whole network collapses.
Row 12: 
Row 13: REWARDS:
Row 14:  • Perfect (0 alerts): ¤18,000 + reputation boost
Row 15:  • Clean (1–2 alerts): ¤15,000
Row 16:  • Compromised (3+ alerts): ¤8,000
Row 17: 
Row 23: ▼ 2 more lines
Row 24: CDR=scroll BACK=exit QUOTE=save EVAL=start
```

---

## 3A. CIPHER-LINE Auxiliary Display

The CIPHER-LINE OLED mounted above the keyboard is a separate rendering surface from the main 80×25 grid. Every Cipher voice utterance is emitted here and only here. Every pattern in §1–§3 above applies to the main grid; the patterns in this section apply to CIPHER-LINE and nowhere else. See `docs/software/runtime/cipher-voice.md` for the grammar engine; see CLAUDE.md Canonical Hardware Specification for the physical display spec.

### 3A.1 Four-Row Layout

CIPHER-LINE is presented to cartridges as four logical rows. The font is rendered at Press Start 2P's native 8×8 (no vertical scaling applied — the OLED's vertical pixel budget allows native-cell rendering distinct from the main grid's 12×24). Each row accommodates ~32 character cells.

```
Row 1  [status strip]                (nOSh-runtime-owned; cart may write via aux-status-render)
Row 2  [Cipher scrollback — line A]  (Cipher-engine-owned; current fragment)
Row 3  [Cipher scrollback — line B]  (Cipher-engine-owned; previous fragment echo)
Row 4  [contextual]                  (nOSh-runtime-owned; cart may write via aux-status-render)
```

**Row 1 (status strip).** nOSh runtime defaults: time of session (MM:SS), link state if applicable (`LNK`), cart tag, operator handle short form, beat marker. Cartridges may override with `(aux-status-render 1 "..." :priority N)`; the nOSh-runtime-owned default resumes when the cart releases the row.

**Rows 2–3 (Cipher scrollback).** nOSh-runtime-exclusive. Cart code cannot write here; the only path to this row pair is `(cipher-emit ...)` and `(cipher-push-event ...)`, which feed the grammar engine. Row 2 is the current fragment. When the engine emits a new fragment, Row 2's contents move up to Row 3 (previous line echo) and the new fragment appears on Row 2. Silent ticks do not cause a shift — Row 2 and Row 3 hold across silent ticks, giving the operator time to read.

**Row 4 (contextual).** nOSh runtime defaults: chord hints relevant to the current main-grid context, swap countdown if an `aux-timer-*` is running, `TERM: CAPTURE` hint when CIPHER-LINE is in seed-capture mode. Cartridges may override with `(aux-status-render 4 "..." :priority N)` — for example, a Marty Glitch contract might pin `STN 88.5 MHz` to Row 4 while a broadcast hijack is active.

### 3A.2 Scroll and Pause Behavior

The scroll model is **echo-pair**, not continuous. Row 2 and Row 3 form a two-line window that updates one row at a time, cadence-matched to Cipher utterances:

- **New non-silent utterance:** Row 3 ← Row 2, Row 2 ← new fragment. One-shot transition at ~120 ms fade (OLED-hardware-friendly; matches rendering loop in ADR-0015).
- **Silent tick:** no movement; Row 2 and Row 3 hold.
- **Repeat-suppressed utterance:** same as silent (the grammar engine rejects duplicates via the coherence-stack check, §9 of the Grammar Framework).
- **Pause on anomaly:** utterances tagged `:anomalous` hold on Row 2 for the duration of the current beat plus 2 seconds before the next utterance can overwrite. This ensures the operator's peripheral registers anomalous beats without rushing.

No horizontal scrolling within Rows 2–3 — if a fragment exceeds ~32 characters, the grammar clips the expansion at load (it's a bug in the grammar if a fragment overflows; the engine logs such cases to boot diagnostic). Row 1 and Row 4 do ticker per §3A.3.

### 3A.3 Row-Overflow Ticker (Rows 1 and 4 Only)

Rows 1 and 4 hold strings that the nOSh runtime or carts write via `aux-status-render`. These rows may legitimately need to carry content longer than 32 chars (long operator handles, verbose beat markers, long contextual hints like `TERM: CAPTURE  //  ANY KEY: DISMISS`).

**Ticker semantics:**

- If the string fits the row budget, render statically.
- If it exceeds the budget, the row enters ticker mode: the string scrolls left-to-right at 2 characters per second, pauses for 1 second at the end, wraps, pauses 1 second at the start, repeats.
- While tickering, the row does not clip — the full string is legible over one ticker cycle (~8 seconds for a 48-char string).
- `aux-status-render` with a new string resets the ticker; priority rules (§11 of Grammar Framework) determine which cart/nOSh-runtime string wins when multiple writers contend.
- Ticker behavior is local to Rows 1 and 4. **Rows 2 and 3 never ticker** — the grammar engine must fit fragments within budget.

### 3A.4 CIPHER-LINE Pattern Catalogue

These are the reusable CIPHER-LINE patterns. They are orthogonal to the main-grid patterns in §1–§3.

#### 3A-Pattern-A: Ambient

The default state. No cart special rendering. Row 1 shows nOSh runtime status. Rows 2–3 show the two most recent non-silent Cipher utterances (possibly stale from a previous beat). Row 4 shows nOSh-runtime-default chord hints for the current main-grid context.

```
Row 1  02:14  LNK  ICE  KINOSHITA  :active-hack
Row 2  trace. watches.
Row 3  node.
Row 4  CAR drill  CDR next  INFO orient
```

#### 3A-Pattern-B: Tense Beat

When beat flips to `:high-tense`, the mode selector biases toward silence. Rows 2–3 hold (may freeze for many seconds). Row 1 may show an intensified status. Row 4 may pin a countermeasure hint.

```
Row 1  02:47  LNK  ICE  KINOSHITA  :high-tense  TRC 32
Row 2  same node. black ice. sector 7.
Row 3  trace. watches.
Row 4  EVAL commit  BACK retreat  QUOTE hot-swap
```

Rows 2–3 may stay like this for 10+ seconds while Cipher elects silence. That is the mode working. The operator feels it.

#### 3A-Pattern-C: Phase Transition

Beat flips to `:phase-transition`. Mode selector biases toward `reflect`. Expect chained utterances over 3–5 ticks that tie current phase to past events.

```
Row 1  03:55  LNK  ICE  KINOSHITA  :phase-transition  P2/3
Row 2  same corridor. the wreck at three-fifty.
Row 3  hull breached. pressure spike.
Row 4  EVAL advance phase
```

#### 3A-Pattern-D: Cart-Swap Lull

Beat flips to `:cart-swap-lull` on pull; beat stays there for 2–5 ticks after the new cart loads. Mode selector biases heavily toward `drift`. Row 1 shows the pre-swap cart's tag briefly, then the new tag. Row 4 pins the swap countdown.

```
Row 1  04:12  LNK  (swap)  KINOSHITA  :cart-swap-lull
Row 2  last one. mirror. black ice. clean.
Row 3  same node. black ice. sector 7.
Row 4  SWAP: 00:05
```

#### 3A-Pattern-E: Seed Capture

Cart calls `(aux-show-seed ...)`. CIPHER-LINE enters seed capture mode. Row 1 displays the seed with a label. Row 4 pins `TERM: CAPTURE`. Rows 2–3 continue their normal echo-pair behavior (Cipher keeps speaking during capture; the seed is Row 1's job). Operator presses TERM to persist the seed to Universal Deck State. See `KN-86-Input-System-Architecture.md` for the interaction flow.

```
Row 1  SEED  A7 F3 91 0C 88 22 5E 14
Row 2  took the handshake.
Row 3  same relay. three sessions back.
Row 4  TERM: CAPTURE   BACK: DISMISS
```

#### 3A-Pattern-F: Silent

Mission-critical silence. Cart or nOSh runtime has explicitly requested `(cipher-emit :silent)` on a reverent beat. Rows 2–3 hold. Row 1 and Row 4 may still update. The deck has chosen to not speak. Do not fill it.

```
Row 1  05:02  LNK  DEP  KINOSHITA  :high-tense
Row 2  same pressure. the wreck at three-fifty.
Row 3  hull breached.
Row 4  EVAL commit  BACK retreat
```

Rows 2–3 here are from 8 seconds ago. The Cipher has not spoken since. That is by design.

### 3A.5 What CIPHER-LINE Is Not For

- **Not** for cartridge-authored text that isn't Cipher voice. If a cart wants to surface data to the operator (mission description, tool readout, contract details), it renders on the main grid, not CIPHER-LINE.
- **Not** for persistent display of main-grid state. Row 1 may summarize, but the canonical state lives on Row 0 of the main grid.
- **Not** a notification center. Critical nOSh runtime notifications use main-grid modals (§3.10) or inline status (§3.1). Cipher reflects on events; it does not alert.
- **Not** a chat log. Rows 2–3 are an ephemeral echo pair, not a scrollback for the operator to review. Extended Cipher review lives in Null (§ Null Gameplay Spec), which reads the memory store and coherence stack into a main-grid panel.

### 3A.6 Separating Main-Grid Patterns from CIPHER-LINE Patterns

Throughout this document, any pattern that used to involve "Cipher voice on the main grid" has been retired or redirected. The separation rules:

| Surface | Patterns |
|---------|----------|
| **Main 80×25 grid** | §1 Screen Architecture, §2 Navigation Grammar, §3 Standard HUD. Patterns 3.1 through 3.11 apply here. Modals (3.10) are for nOSh runtime prompts, not Cipher. |
| **CIPHER-LINE OLED** | §3A above. Patterns 3A-A through 3A-F apply here. Cartridges never draw pixels; they only push events and contribute grammar. |

Gameplay specs that previously placed Cipher utterances in main-grid modals, main-grid scrolling text areas, or dialogue boxes have been rewritten to move those beats to CIPHER-LINE — see the module specs in `docs/gameplay-specs/` for each module's new **CIPHER-LINE Contributions** section (vocabulary pool, production fragments, mode-weight biases the cart declares).

---

## 4. Typography & Symbolism

### 4.1 Character Conventions

**Currency:**
```
¤ = Currency symbol (e.g., ¤8,400)
```

**Numeric Formatting:**
```
1,234 = Thousands separator (commas)
12.34 = Decimal values (e.g., ¤12.34 rare; usually ¤12 or ¤1,234)
1% = Percentage sign
```

**Time Format:**
```
MM:SS = Minutes:Seconds (e.g., 8:45)
H:MM = Hours:Minutes (e.g., 2:34, not 02:34)
HH:MM:SS = Full format (e.g., 1:23:45) for long-running contracts
```

**Module Abbreviations (tight spaces, 4 chars max):**
```
ICEBREAKER     → ICE  (network intrusion)
SIGNAL         → SIG  (signal tracking, decryption)
LEDGER         → LEG  (forensic accounting)
GRID           → GRD  (spatial operations)
VAULT          → VLT  (knowledge index)
CIPHER         → CIP  (cryptographic tools)
DEPTHCHARGE    → DCG  (maritime reconnaissance)
DRIFT          → DRF  (signal tracking, secondary)
```

**Status Abbreviations:**
```
ACTIVE   → ACT
PENDING  → PND
ARCHIVE  → ARC
FAILED   → FAI
SUCCESS  → SUC
LOCKED   → LCK
```

---

### 4.2 Threat Level Visual Vocabulary

**5-Level Escalation (combined text + bar):**

**Level 1 (Green, Safe):**
```
THREAT ░░░░░░░░░░ 1/5 SAFE
```

**Level 2 (Yellow, Caution):**
```
THREAT ██░░░░░░░░ 2/5 CAUTION
```

**Level 3 (Orange, Alert):**
```
THREAT ████░░░░░░ 3/5 ALERT
```

**Level 4 (Red, Danger):**
```
THREAT ██████░░░░ 4/5 DANGER
```

**Level 5 (Critical):**
```
THREAT ██████████ 5/5 CRITICAL
```

**ICE Type Threat Codes (single-letter):**
```
J = JUNK ICE (threat 1)
B = BLACK ICE (threat 2–3)
R = RED ICE (threat 4–5)
H = HUNTER ICE (threat 4–5, mobile)
```

---

### 4.3 Glyphs & Icon Sets

**Selection & Focus:**
```
[ ] = Unselected item
[>] = Selected item (focused)
[◊] = Bookmarked / Flagged item
[*] = Active / Current item
[+] = New / Added item
```

**Status Indicators:**
```
[■] = Active / Loaded / Running
[□] = Inactive / Unloaded / Idle
[◈] = Processing / Loading / Transitioning
[!] = Alert / Warning / Exception
[×] = Error / Failed / Locked
[√] = Success / Verified / Complete
```

**Navigation Arrows (in nested/tree contexts):**
```
[▶] = Collapsed (can expand)
[▼] = Expanded (can collapse)
> = Next item (in progress indicators)
```

**Grid Cells (Pattern 3):**
```
[░] = Empty / Open / Passable
[█] = Occupied / Locked / Blocked
[+] = Node / Waypoint / Objective location
[*] = Special / Important / Starting position
[!] = Hazard / Warning / Danger zone
[-] = Connection / Link / Path
[~] = Uncertain / Shadowed / Unknown
```

---

### 4.4 Key Naming (for display in action bars)

**Canonical Key Names (used in action bar):**
```
CAR     (enter, drill)
CDR     (next, traverse)
CONS    (combine, link)
NIL     (clear, discard)
EVAL    (execute, confirm) — usually rendered as just "EVAL"
QUOTE   (bookmark, defer)
LAMBDA  (macro record) — rare in UI hints; usually hidden
APPLY   (use, deploy)
ATOM    (test atomic) — rare in UI hints
EQ      (compare) — rare in UI hints
BACK    (return, cancel, abort)
INFO    (inspect, observe, detail)
LINK    (transmit, egress)
SYS     (system menu, hard cancel)
```

**Numpad Keys (rendered as numbers):**
```
0–9, ., ENT (enter), +, -, *, /
```

---

## 5. Sound Design Palette

### 5.1 Standard Audio Events

All cartridges share a common audio event vocabulary, routed through YM2149 Voice 1, 2, and 3.

**Voice Channel Assignment (across all modules):**
- **Voice 1:** Navigation / UI feedback (key presses, cursor movement)
- **Voice 2:** Gameplay / Action events (attacks, tool use, status changes)
- **Voice 3:** Ambient / Tension (threat level, time pressure, background)

### 5.2 Kepress Confirmation Tones

**Navigation Keys (CDR, cursor movement):**
- Frequency: 1.0–1.2 kHz
- Duration: 50ms
- Envelope: Attack=5ms, Release=45ms (short, crisp)
- Channel: Voice 1
- Example: `1.0 kHz 50ms`

**Confirm/Drill Keys (CAR, EVAL, APPLY):**
- Frequency: 1.5 kHz
- Duration: 100ms
- Envelope: Attack=5ms, Sustain=70ms, Release=25ms (slightly longer)
- Channel: Voice 1
- Example: `1.5 kHz 100ms`

**Error/Cancel (NIL, BACK, SYS):**
- Frequency: 800 Hz
- Duration: 100ms
- Envelope: Attack=5ms, Release=95ms (descending feel)
- Channel: Voice 1
- Example: `800 Hz 100ms`

**Special Keys (QUOTE, CONS, LINK):**
- Frequency: 1.8 kHz
- Duration: 80ms
- Envelope: Attack=2ms, Release=78ms (high and quick)
- Channel: Voice 1
- Example: `1.8 kHz 80ms`

---

### 5.3 Gameplay Audio Events

**(These are cartridge-specific; here are common archetypes)**

**Success Tone:**
- Frequency sequence: 1.2 kHz (100ms) → 1.5 kHz (100ms)
- Total: 200ms
- Envelope: Smooth transitions
- Channel: Voice 2

**Failure Tone:**
- Frequency sequence: 1.5 kHz (100ms) → 1.0 kHz (100ms) → 800 Hz (100ms)
- Total: 300ms (descending)
- Envelope: Smooth transitions
- Channel: Voice 2

**Alert/Escalation:**
- Frequency: 1.8 kHz
- Duration: 150ms
- Envelope: Attack=20ms, Release=130ms (builds then fades)
- Pitch bend (optional): Start at 1.5 kHz, rise to 1.8 kHz over 150ms
- Channel: Voice 2

**Threat Indicator (looping, for tension):**
- Frequency: 1.0–1.5 kHz (sweeping)
- Duration: ~500ms per cycle
- Envelope: Continuous, no decay
- Channel: Voice 3 (ambient)

---

### 5.4 Phase Transition Sounds

**Phase Completion (user reached objective):**
- Frequency sequence: 1.2 kHz → 1.5 kHz → 1.8 kHz
- Duration per note: 150ms
- Total: 450ms (ascending "victory" chord)
- Channel: Voice 2

**Phase Start (entering new phase):**
- Frequency: 1.2 kHz
- Duration: 100ms (single tone)
- Followed by: 200ms silence
- Followed by: Cartridge-specific intro tone (varies per module)
- Channel: Voice 1 (brief), then Voice 2 (intro)

**Hot Swap Notification (physical cartridge swap required):**
- Frequency sequence: 800 Hz (100ms) → 1.2 kHz (100ms) → 1.5 kHz (100ms)
- Total: 300ms (ascending, distinct from success)
- Channel: Voice 1

---

### 5.5 Module-Specific Channels

Each cartridge reserves Voice assignments for its own gameplay:

**Example: ICE Breaker**
- Voice 1: Navigation (shared)
- Voice 2: Combat events (tool attacks, ICE counterattacks)
- Voice 3: Threat scanner (sweeping tone, escalates with threat level)

**Example: Black Ledger**
- Voice 1: Navigation (shared)
- Voice 2: Transaction alerts, payout notifications
- Voice 3: Ambient ticking clock (time pressure)

**Example: NeonGrid**
- Voice 1: Navigation (shared)
- Voice 2: Movement/action feedback
- Voice 3: Ambient spatial tone (depth/distance representation)

---

## 6. Interaction State Machine

### 6.1 Standard State Transitions

**Standard Flow:**
```
START (boot)
  ↓
IDLE (waiting for input)
  ↓
[User presses key]
  ↓
INPUT_DISPATCH (key routing)
  ├→ Navigation Key (CDR, CAR, etc.)
  │   └→ UPDATE_STATE (move cursor, change selection)
  │   └→ AUDIO_FEEDBACK (key tone)
  │   └→ RENDER (redraw screen)
  │   └→ IDLE
  │
  ├→ Action Key (EVAL, APPLY)
  │   └→ EXECUTE_ACTION (cartridge handler)
  │   └→ AUDIO_FEEDBACK (action tone)
  │   └→ STATE_CHANGE (new phase, new view, etc.)
  │   └→ IDLE or WAIT_RESULT
  │
  └→ System Key (BACK, SYS, INFO)
      └→ SYSTEM_HANDLER (return, menu, inspect)
      └→ AUDIO_FEEDBACK
      └→ IDLE

WAIT_RESULT (async operation)
  ├→ OPERATION_COMPLETE
  │   └→ UPDATE_STATE (apply result)
  │   └→ AUDIO_FEEDBACK
  │   └→ RENDER
  │   └→ IDLE
  │
  └→ OPERATION_TIMEOUT (if time-limited)
      └→ FAIL_HANDLER
      └→ AUDIO_FEEDBACK (error tone)
      └→ IDLE
```

---

### 6.2 List Navigation State Machine

**Context: User browsing a contract list (Pattern 1)**

```
STATE: LIST_VIEW
├─ data = list of contracts
├─ cursor_position = 0 (first item)
├─ view_offset = 0 (window start in list)

KEY: CDR (next)
  ├─ IF cursor_position < list.length - 1:
  │    └─ cursor_position += 1
  │    └─ AUDIO: navigation tone (1.0 kHz, 50ms)
  │    └─ IF cursor_position > view_offset + 19:
  │        └─ view_offset += 1  (scroll window)
  │    └─ RENDER (redraw list with new cursor position)
  │
  └─ ELSE (at end of list):
      └─ cursor_position = 0 (wrap to top)
      └─ view_offset = 0
      └─ AUDIO: boundary tone (1.5 kHz, 100ms, pitch slightly lower)
      └─ RENDER

KEY: CAR (enter)
  └─ AUDIO: confirm tone (1.5 kHz, 100ms)
  └─ PHASE_CHANGE: LIST_VIEW → DETAIL_VIEW (inspector)
  └─ RENDER (new screen: contract details)

KEY: INFO (inspect)
  └─ IF single-tap AND info_available:
  │    └─ DISPLAY_SUMMARY (action bar shows brief summary)
  │    └─ NO STATE CHANGE
  │
  └─ ELSE IF double-tap:
      └─ PHASE_CHANGE: LIST_VIEW → DETAIL_VIEW (full inspector)
      └─ RENDER (pattern 5)

KEY: BACK (return)
  └─ AUDIO: cancel tone (800 Hz, 100ms)
  └─ PHASE_CHANGE: LIST_VIEW → PARENT_VIEW
  └─ RENDER (mission board or hub)

KEY: QUOTE (bookmark)
  └─ AUDIO: special tone (1.8 kHz, 80ms)
  └─ bookmarks.add(contracts[cursor_position])
  └─ RENDER (mark with [◊] instead of [ ])
  └─ STATE: LIST_VIEW (unchanged)
```

---

### 6.3 Modal Interrupt State Machine

**Context: nOSh-runtime-level prompt interrupts current gameplay.** Typical cases: hot-swap request, confirmation prompt, critical system alert. **Cipher utterances do NOT interrupt.** Cipher renders to CIPHER-LINE without stealing focus from the main grid (see §3A CIPHER-LINE) — that is the whole point of the auxiliary surface.

```
STATE: ACTIVE_PHASE (user in mission)
  └─ Current screen: Pattern 6 combat or Pattern 5 inspector

INTERRUPT: NOSh_Runtime_Prompt_Required  (hot-swap, confirmation, alert)
  ├─ audio_queue: prompt tone
  ├─ PAUSE_INPUT (user cannot interact with main screen)
  ├─ STATE: MODAL_DISPLAYED
  │   ├─ overlay: nOSh runtime prompt (Pattern 3.10 modal)
  │   ├─ action_bar: "EVAL=continue"
  │   └─ AUDIO: fade background (lower volume -6dB), play prompt tone
  │
  └─ KEY: EVAL (continue)
      ├─ AUDIO: confirm tone (1.5 kHz, 100ms)
      ├─ STATE: ACTIVE_PHASE (resume)
      ├─ RESUME_INPUT
      ├─ AUDIO: restore background volume
      └─ RENDER (main screen restored)

ALT: KEY: SYS (hard cancel, hold 2 seconds)
  ├─ AUDIO: alarm tone (1.0 kHz rising to 1.8 kHz over 2 seconds)
  ├─ STATE: SYSTEM_MENU
  └─ OPTIONS: Abort mission, save, exit

PARALLEL (non-interrupting): Cipher_Utterance
  └─ CIPHER-LINE renders per §3A CIPHER-LINE.
     No pause. No focus shift. Main grid is untouched.
```

---

### 6.4 Split Pane Focus Switch

**Context: Black Ledger dual-pane transaction view**

```
STATE: DUAL_PANE_VIEW
├─ left_pane: Account A transactions
├─ right_pane: Account B transactions
├─ focused_pane: LEFT (default)
├─ left_cursor: 0
├─ right_cursor: 0

KEY: CDR (traverse)
  ├─ IF focused_pane == LEFT:
  │    └─ left_cursor += 1 (advance in left)
  │    └─ AUDIO: nav tone (1.0 kHz, 50ms)
  │    └─ RENDER (left pane redraws)
  │
  └─ ELSE (focused_pane == RIGHT):
      └─ right_cursor += 1
      └─ AUDIO: nav tone
      └─ RENDER (right pane redraws)

KEY: CAR (enter)
  └─ IF focused_pane == LEFT:
  │    └─ Inspect left item (Inspector pattern 5)
  │
  └─ ELSE:
      └─ Inspect right item

KEY: CONS (link)
  ├─ IF left_cursor > -1 AND right_cursor > -1:
  │    └─ AUDIO: link tone (descending pair: 1.2 → 800 Hz, 150ms)
  │    └─ link_table.add(left_item, right_item)
  │    └─ RENDER (mark both selections as linked)
  │
  └─ ELSE:
      └─ AUDIO: error tone (800 Hz, 100ms)
      └─ ACTION_BAR: "Select items in both panes to link"

ALT: EQ (switch focus)
  ├─ focused_pane = (focused_pane == LEFT) ? RIGHT : LEFT
  ├─ AUDIO: focus switch tone (1.5 kHz, 100ms)
  └─ RENDER (action bar updates to show new focused pane)
```

---

### 6.5 Grid Navigation & Targeting

**Context: ICE Breaker node map or NeonGrid spatial operations**

```
STATE: GRID_VIEW
├─ grid: 8×8 node array
├─ cursor_x: 4 (center)
├─ cursor_y: 4 (center)

KEY: CDR (cycle adjacent)
  ├─ CYCLE: [cursor.x, cursor.y] → next cell (clockwise)
  ├─ AUDIO: spatial scan tone (pitch based on position)
  └─ RENDER (highlight new cell with border)

Alternative: Cardinal Movement (if supported)
  ├─ UP: cursor_y -= 1 (wraps if at top)
  ├─ DOWN: cursor_y += 1 (wraps if at bottom)
  ├─ LEFT: cursor_x -= 1 (wraps if at left)
  ├─ RIGHT: cursor_x += 1 (wraps if at right)
  └─ AUDIO: different tone per direction (ascending right, descending left, etc.)

KEY: CAR (enter cell)
  ├─ grid[cursor_y][cursor_x].on_enter()
  ├─ IF cell.has_content:
  │    └─ PHASE_CHANGE: GRID_VIEW → DETAIL_VIEW or COMBAT (Pattern 6)
  │
  └─ ELSE:
      └─ AUDIO: blocked tone (1.0 kHz, 100ms)
      └─ RENDER (cursor stays on current cell)

KEY: INFO (scan)
  └─ DISPLAY: Cell details in action bar or modal
  │    └─ "JUNK ICE, STRENGTH 2, NO SUBDIR, SAFE"
  └─ STATE: GRID_VIEW (unchanged)

KEY: QUOTE (bookmark)
  └─ AUDIO: special tone (1.8 kHz, 80ms)
  └─ grid[cursor_y][cursor_x].bookmarked = true
  └─ RENDER (mark cell with [◊] if uses symbol, or border change)

KEY: ATOM (test terminal)
  └─ IF cell.is_terminal:
  │    └─ AUDIO: success tone (ascending)
  │    └─ ACTION_BAR: "TERMINAL NODE"
  │
  └─ ELSE:
      └─ AUDIO: failure tone (descending)
      └─ ACTION_BAR: "NODE HAS CHILDREN"
```

---

### 6.6 Error Handling & Recovery

**State: Operation Fails (e.g., attack rejected, data corrupted)**

```
STATE: ACTIVE_PHASE (user executing action)

ACTION: CAR (attack node with wrong tool)
  ├─ node.on_attack(current_tool)
  ├─ result: FAIL (wrong tool type)
  │
  ├─ AUDIO: error tone (800 Hz, 100ms, low pitch)
  ├─ STATE: ERROR_DISPLAY (temporary modal)
  │   ├─ overlay: "[!] ATTACK FAILED: Tool ineffective against this ICE"
  │   └─ action_bar: "EVAL=continue"
  │
  └─ KEY: EVAL (acknowledge)
      ├─ AUDIO: confirm (1.5 kHz, 100ms)
      ├─ STATE: ACTIVE_PHASE (resume)
      ├─ CLOSE_MODAL
      └─ RENDER (main view unchanged)

STATE: Operation Timeout

ACTION: Time limit exceeded during mission
  ├─ mission.time_remaining <= 0
  ├─ AUDIO: escalating alarm (frequency rises from 1.0 → 1.8 kHz over 300ms)
  ├─ STATE: TIMEOUT_MODAL
  │   ├─ overlay: "[!] TIME EXPIRED: Mission collapse imminent"
  │   └─ action_bar: "EVAL=continue BACK=abort"
  │
  ├─ KEY: EVAL (accept consequences)
  │    └─ mission.payout *= 0.5  (half payout for timeout)
  │    └─ mission.reputation *= 0.75  (reputation penalty)
  │    └─ PHASE_CHANGE: DEBRIEF
  │
  └─ KEY: BACK (abort mission)
      └─ mission.payout = 0
      └─ mission.reputation *= 0.5  (failure penalty)
      └─ PHASE_CHANGE: MISSION_BOARD
```

---

## 7. Cartridge-Specific Customization Points

### 7.1 Mandatory Standard Components

These elements MUST be consistent across all cartridges. Cartridges may NOT customize them:

1. **Status Bar (Row 0):** Operator handle, credits, reputation, threat/time. Format defined in section 3.1. Cartridge can add ONE module-specific indicator (e.g., alert level, connection strength).

2. **Mission Board & Contract Selection:** The nOSh runtime owns the mission board UI. All contracts display in Pattern 1 list with standard columns: threat level, payout, phase count. Cartridge templates define CONTRACT_CELL content; UI frame is standard.

3. **Phase Chain Navigation:** The nOSh runtime owns phase transitions, cartridge swaps (if multi-phase), debrief screen. Cartridge cannot customize the hot swap modal or phase numbering.

4. **Cipher Voice Rendering (CIPHER-LINE):** The nOSh runtime owns the Cipher engine and renders every utterance onto CIPHER-LINE (§3A CIPHER-LINE). Cartridges contribute a `cipher-grammar` block (vocabulary, production fragments, mode-weight biases) per `docs/software/runtime/cipher-voice.md`. Cartridges never draw Cipher text on the main grid and never render pixels to CIPHER-LINE directly — that is the nOSh runtime's rendering loop.

5. **Action Bar (Row 24):** Format and key hints are standardized. Cartridge can customize hints' text but must adhere to 80-char limit and avoid inventing new key bindings.

6. **Deck State & Economy:** All cartridges use universal credits (¤), reputation (0–50+), and cartridge history bitfield. Cartridge cannot introduce parallel currencies or economies.

---

### 7.2 Highly Customizable Components

Cartridges have full freedom in these areas:

1. **Active Phase Display (Rows 1–23):** Cartridge owns the mission content. Can use any of the 6 screen patterns or invent new patterns (but keep them within the 80×25 grid and text-mode paradigm). Audio design is fully customizable (respecting Voice 1–3 channel conventions).

2. **Contract Details (Detail Inspector, Pattern 5):** Cartridge defines what fields appear in contract inspector (ICE Breaker: threat level, network size, countermeasure info; Black Ledger: entity profiles, transaction count, complexity).

3. **Sound Design (Cartridge-Specific):** Voice 2 and Voice 3 are reserved for the cartridge's gameplay. Define unique tones for attacks, alerts, status changes, etc. Voice 1 (navigation) remains shared/standard.

4. **Cell Architecture:** Cartridge defines its own cell types (NODE_CELL, TOOL_CELL, TRANSACTION_CELL, etc.) with custom ON_CAR, ON_CDR, ON_EVAL handlers. The cartridge grammar defines the framework; cartridge code provides the behavior. (The CIPHER-LINE Grammar Framework is a separate, auxiliary-surface spec — see `docs/software/runtime/cipher-voice.md`.)

5. **Procedural Generation:** Cartridge templates and generation algorithms are proprietary. The mission board framework (nOSh) instantiates contracts from cartridge templates; the template structure is up to the cartridge.

---

### 7.3 Customization Examples

**ICE Breaker (Network Intrusion):**
- **Standard:** Mission board, contract selection, status bar (with ALERT level added)
- **Custom:** Network map display (Pattern 3 grid), node combat (Pattern 6 real-time), threat scanner audio (Voice 3)

**Black Ledger (Forensic Accounting):**
- **Standard:** Mission board, contract inspector, status bar
- **Custom:** Dual-pane transaction view (Pattern 2), entity linking (CONS key semantics), transaction detail inspector

**NeonGrid (Spatial Operations):**
- **Standard:** Mission board, status bar
- **Custom:** Spatial grid display (Pattern 3), cardinal movement (not just CDR spiraling), 3-view toggle (grid/tactical/sonar), threat heatmap audio

**The Vault (Knowledge Index):**
- **Standard:** Mission board, status bar
- **Custom:** Hierarchical tree navigation (nested Pattern 1 lists), keyword search, data visualization (ASCII tree diagrams)

---

## 8. Reference Implementation: ICE Breaker

This section demonstrates how a cartridge implements the design system.

### 8.1 ICE Breaker Screen Layouts

**Screen 1: Mission Board (nOSh Standard)**
```
Row 0: NEXUS          ¤8,400 (5,000/day)   [PHASE 0/0]  TIME IDLE    REP 8   THR ░░░░░░░░░░
Row 1: AVAILABLE CONTRACTS
Rows 2–22: [List Pattern 1]
       [>] INFILTRATE SIGMA NODE                T4 ¤15,000
       [ ] FORENSIC DIVE: CIPHER CORP             T2 ¤8,000
       ... (more contracts)
Row 24: CAR=enter CDR=next BACK=exit INFO=detail EVAL=accept
```

**Screen 2: Contract Inspector (Cartridge Custom)**
```
Row 0: NEXUS          ¤8,400 (5,000/day)   [PHASE 0/0]  TIME IDLE    REP 8   THR ░░░░░░░░░░
Row 1: CONTRACT: INFILTRATE SIGMA NODE
Row 2: ────────────────────────────────────────────────────────────────────────────────
Row 3: Threat Level.............................4/5 (RED ICE)
Row 4: Network Size.............................8 nodes
Row 5: Time Limit...............................8:00
Row 6: Payout...................................¤15,000
Row 7: Phase....................................1/1
Row 8: 
Row 9: OBJECTIVE:
Row 10: Retrieve encryption keys from node 7. Extract cleanly.
Row 11: 
Row 12: ICE CONFIGURATION:
Row 13:  > 2× BLACK ICE (nodes 2, 5)
Row 14:  > 2× JUNK ICE (nodes 1, 3, 4, 6)
Row 15:  > 1× RED ICE (node 7 — exit guarded)
Row 16: 
Row 17: COUNTERMEASURES:
Row 18:  > HUNTER spawns after 3 node revisits
Row 19:  > Alerts triggered on detection (2 permits collapse)
Row 20: 
Row 21: REWARD SCALING:
Row 22:  > Perfect (0 alerts): ¤18,000 + rep
Row 23: [CDR for more | EVAL to accept]
Row 24: CAR=accept BACK=cancel QUOTE=defer CDR=scroll
```

**Screen 3: Node Combat (Cartridge Custom, Pattern 6)**
```
Row 0: NEXUS          ¤15,000             [PHASE 1/1]  TIME 2:34    REP 8   ICE ████░░░░░░
Row 1: NODE 5 — BLACK ICE DEFENSE
Rows 2–18: Combat encounter
       [JUNK] ─→ [BLACK*] ─→ [JUNK]
       (connected nodes)
        ↓        ↓          ↓
       [░]     [██] STR 3  [░]
Row 19: TIME ██████░░░░ 2:34
Row 20: THREAT ████░░░░░░ 4/5
Row 21: SLICER ████████░░ (6/8 loaded)
Row 22: 
Row 24: CAR=attack CDR=target APPLY=slicer EVAL=exploit BACK=retreat
```

**Screen 4: Network Overview (Cartridge Custom, Pattern 3 grid)**
```
Row 0: NEXUS          ¤15,000             [PHASE 1/1]  TIME 5:15    REP 8   ICE ██░░░░░░░░
Row 1: NETWORK TOPOLOGY: SIGMA NODE
Rows 2–23: 8×8 node grid
       [S] [B] [J] [B] [J] [░] [░] [X]
       [░] [█] [█] [█] [█] [░] [░] [░]
       [░] [█] [G] [░] [░] [░] [█] [█]
       [░] [░] [░] [░] [R] [X] [░] [░]
       
       [S] = start
       [G] = goal (node 7)
       [X] = exit
       [B] = BLACK ICE
       [J] = JUNK ICE
       [R] = RED ICE
       [█] = firewall/wall
       [░] = empty/open
Row 24: CAR=enter CDR=move INFO=scan QUOTE=mark BACK=exit
```

---

### 8.2 Audio Events (ICE Breaker)

**Voice 1 (Navigation) — Shared:**
- CDR (move cursor): 1.0 kHz, 50ms
- CAR (enter node): 1.5 kHz, 100ms
- BACK (retreat): 800 Hz, 100ms

**Voice 2 (Combat) — Custom:**
- Attack with Slicer: 1.2 kHz, 150ms (crisp)
- Attack with Cracker: 1.0 kHz, 200ms (slower)
- Attack with Worm: 1.5 kHz descending to 1.0 kHz, 200ms (sweep)
- ICE counterattack: 1.8 kHz, 100ms + burst of noise
- Tool depleted: 800 Hz, 150ms (error)

**Voice 3 (Threat) — Custom:**
- Threat scan: 1.0 kHz rising to 1.8 kHz, 500ms loop (spatial sweep)
- Hunter spawned: Pulsing alarm, 400 Hz, 200ms on/off

---

## 9. Cross-Module Interaction Patterns

### 9.1 Shared Data Structures

**Universal Deck State (64 bytes SRAM):**
```
struct DeckState {
  uint8_t operator_handle[12];      // e.g., "NEXUS"
  uint32_t credit_balance;          // ¤ (universal currency)
  uint8_t reputation;               // 0–50+ (cumulative)
  uint16_t cartridge_history;       // Bitfield: which cartridges have been used
  uint16_t current_phase_chain;     // Phase sequence for multi-phase missions
  uint8_t lambda_slots[8][32];      // 8 macros × 32 steps each
  uint8_t quote_slots[16];          // 16 bookmarks
  uint8_t cipher_seed;              // Seed for Cipher voice generation
};
```

**Cartridge History Bitfield (16 bits, one per module):**
```
Bit 0  = ICE Breaker
Bit 1  = Depthcharge
Bit 2  = Shellfire
Bit 3  = Takezo
Bit 4  = Black Ledger
Bit 5  = SynthFence
Bit 6  = Drift
Bit 7  = Pathfinder
Bit 8  = Nodespace
Bit 9  = NeonGrid
Bit 10 = The Vault
Bit 11 = Cipher Garden
Bit 12 = Null
Bit 13 = Relay
Bits 14–15 = (reserved)
```

When a cartridge is first loaded, its bit is set in cartridge_history. Mission board uses this to unlock multi-phase contracts that require specific modules.

---

### 9.2 Cross-Module Mission Examples

**Example 1: Multi-Phase Contract Spanning ICE Breaker + Signal**

**Phase 1 (ICE Breaker):** Infiltrate network, retrieve encrypted payload.
- Contract template: `INFILTRATE_SIGMA_NODE`
- Cartridge: ICE Breaker
- Mission board shows: "PHASE 1/2: Infiltrate — Requires ICE Breaker"

**Phase 2 (Signal):** Decrypt payload using signal module.
- Contract template: `DECRYPT_PAYLOAD`
- Cartridge: Signal (SIGNAL — not yet designed; placeholder)
- Mission board shows: "PHASE 2/2: Decrypt — Requires SIGNAL"

**Phase Chain Serialization (nOSh runtime):**
```
phase_chain[0] = {
  module_id: ICE_BREAKER,
  template_id: INFILTRATE_SIGMA_NODE,
  status: ACTIVE
};
phase_chain[1] = {
  module_id: SIGNAL,
  template_id: DECRYPT_PAYLOAD,
  status: PENDING
};
```

**User Flow:**
1. Mission board shows contract with "2 PHASES" indicator
2. User presses CAR → Accept contract → Phase 1 begins (ICE Breaker loads)
3. Phase 1 gameplay → User completes objective → Debrief screen
4. The nOSh runtime detects phase_chain[1].status == PENDING
5. Hot Swap modal appears: "PHASE 2/2: DECRYPT PAYLOAD — Requires SIGNAL"
6. User selects SIGNAL module (simulated cartridge swap)
7. The nOSh runtime transitions to phase 2 → Signal module initializes
8. Phase 2 gameplay → Completion → Debrief
9. Mission complete → Payout + Reputation awarded to deck state

---

### 9.3 Interaction Example: ICE Breaker + Black Ledger

**Scenario:** User completes an ICE Breaker contract that pays ¤15,000. The reputation tier unlocks a high-level Black Ledger contract requiring forensic tracking of the same target.

**Step 1: ICE Breaker Debrief**
```
[√] INFILTRATION SUCCESSFUL
─────────────────────────────────
Payout: ¤15,000
Reputation: +3 (tier 2 advancement)
────────────────────────────────────
[EVAL=collect] [QUOTE=analyze]
```

**Step 2: Deck State Update**
```
deck_state.credit_balance += 15000;     // ¤8,400 → ¤23,400
deck_state.reputation += 3;             // 8 → 11 (crosses tier threshold)
deck_state.cartridge_history |= (1 << ICE_BREAKER_BIT);
```

**Step 3: Return to Mission Board**
- The nOSh runtime regenerates mission board
- New high-threat Black Ledger contracts now appear (gated by rep >= 10)
- One contract references the infiltrated target: "TRACE SIGMA SHELL CORP ASSETS"

**Step 4: Multi-Phase Opportunity**
- Black Ledger mission template includes RELATED_MODULE: ICE_BREAKER
- Contract detail shows: "This target was first identified in [linked to previous mission]. Cross-module investigation available."

---

## 10. Summary & Quick Reference

### 10.1 Pattern Selection Matrix

| Task | Pattern | Row Allocation | Navigation |
|------|---------|-----------------|------------|
| Browse contracts, items, logs | 1. List Browser | Rows 2–22 (21 items) | CDR=cycle, CAR=enter, BACK=exit |
| Compare two entity streams | 2. Split Pane | Rows 2–22 (left/right) | CDR=navigate focused pane, CONS=link |
| Spatial layout, topology, grid | 3. Full Grid | Rows 2–18 (grid cells) | CDR=move adjacent, CAR=action, INFO=scan |
| Real-time status overview | 4. Status Dashboard | Rows 1–23 (sections) | CDR=scroll sections, CAR=detail |
| Deep inspection, narrative, detail | 5. Inspector | Rows 1–22 (focused) | CDR=scroll, CAR=accept, BACK=cancel |
| Time-pressured action, combat | 6. Combat/Action Grid | Rows 2–20 (action arena) | CDR=target, CAR=attack, EVAL=special |

### 10.2 Audio Event Checklist

- [ ] Navigation key tone (CDR, cursor): 1.0 kHz, 50ms
- [ ] Confirm key tone (CAR, EVAL): 1.5 kHz, 100ms
- [ ] Cancel key tone (BACK, NIL): 800 Hz, 100ms
- [ ] Special key tone (QUOTE, CONS): 1.8 kHz, 80ms
- [ ] Modal open: Ascending tone (1.2 → 1.5 kHz)
- [ ] Modal close: Descending tone (1.2 → 800 Hz)
- [ ] Error feedback: 800 Hz, 150ms
- [ ] Success feedback: Ascending sequence (1.2 → 1.5 → 1.8 kHz)
- [ ] Phase transition: Unique module-specific tone

### 10.3 Mandatory Components Checklist

- [ ] Row 0: Status bar (operator handle, credits, reputation, threat/time)
- [ ] Row 24: Action bar (context-sensitive key hints, 80 chars max)
- [ ] Rows 1–23: Content area (screen pattern of choice)
- [ ] Selection cursor: `[>]` for active, `[ ]` for inactive, `[◊]` for bookmarked
- [ ] Title row: Module/screen title
- [ ] Dividers: Horizontal lines (dashes) between sections
- [ ] Key binding consistency: CAR=enter, CDR=next, BACK=exit, EVAL=confirm

### 10.4 Reserved Key Semantics

| Key | Must Meaning | Must Not Mean |
|-----|--------------|---------------|
| CAR | Enter/drill/confirm single action | Cancel, go back, undo |
| CDR | Next/traverse/advance | Previous, go back |
| CONS | Combine/link two elements | Separate, delink |
| NIL | Clear/discard/empty | Confirm, proceed |
| EVAL | Execute/confirm big action | Navigate, preview |
| QUOTE | Bookmark/defer/mark | Execute, finalize |
| BACK | Return to parent/previous | Enter, drill, proceed |
| INFO | Inspect/observe/detail (summary) | Execute, confirm |
| APPLY | Use tool/item/consumable | Confirm main action |
| ATOM | Test if terminal/leaf (query) | Modify, change state |
| EQ | Compare two items (query) | Modify, link, combine |
| LINK | Transmit/egress/save externally | Local action, internal |

---

## 11. Implementation Notes for Developers

### 11.1 Grid Width Reference

When designing screens with table columns or split panes:
- **Total width:** 80 characters (cols 0–79)
- **Status bar (row 0):** 15 + 15 + 12 + 10 + 9 + 18 = 79 chars (leaves 1 margin)
- **List item (pattern 1):** Selection (2) + Name (58) + Secondary (20) = 80 chars
- **Split pane (pattern 2):** Left (39) + Divider (1) + Margin (1) + Right (38) = 79 chars
- **Inspector (pattern 5):** Key (30) + Divider (1) + Value (48) = 79 chars

### 11.2 Row Height Reference

When allocating screen space:
- **Status bar:** Row 0 (fixed, always present)
- **Action bar:** Row 24 (fixed, always present)
- **Content area:** Rows 1–23 (23 rows available)
  - Title: Row 1 (or shared row 1 with tab strip)
  - Divider: 1 row (optional)
  - Content: Remaining rows (can be 20–22 rows)

### 11.3 Monochrome Aesthetics

Since the KN-86 is amber-on-black monochrome:
- Avoid assuming color. Use symbols, spacing, and typography to distinguish elements.
- Highlight active/selected items with prefixes (`[>]`, `[◊]`) or borders (box-drawing chars).
- Use box-drawing characters (┌─┐ etc.) to create visual hierarchy, not color.
- Indent nested content (2 or 3 spaces per level) for depth perception.

### 11.4 Accessibility (text-only environment)

- Key names (CAR, CDR) should always be spelled out in action bars.
- Status indicators should be text labels, not color-only. E.g., "THREAT 4/5 DANGER" not just a red bar.
- Narrative text on the main grid (mission briefings, technical logs) should be readable at ~70 chars per line (natural reading width). Cipher utterances render on CIPHER-LINE (§3A) and are budget-limited to ~32 chars per row; their readability tuning is owned by the CIPHER-LINE Grammar Framework.
- Avoid using only visual symbols; pair with text context.

---

## 12. Appendices

### Appendix A: Typography Reference

**Box Drawing Characters:**
```
Single-line: ┌─┬─┐
             ├─┼─┤
             └─┴─┘

Double-line: ╔═╦═╗
             ╠═╬═╣
             ╚═╩═╝

Mixtures:    ┌─┬─┐     (top single, right double)
             ├─╫─┤
             └─┴─┘
```

**Fill Characters:**
```
█ U+2588 FULL BLOCK (100% filled)
▓ U+2593 DARK SHADE (75%)
▒ U+2592 MEDIUM SHADE (50%)
░ U+2591 LIGHT SHADE (25%)
```

**Other Useful Glyphs:**
```
▶ U+25B6 PLAY (next, collapsed)
▼ U+25BC PLAY DOWN (expanded)
◈ U+25C8 DIAMOND (processing)
◊ U+25CA DIAMOND OUTLINE (bookmarked)
√ U+221A SQUARE ROOT (success)
× U+00D7 MULTIPLICATION SIGN (error, locked)
→ U+2192 RIGHTWARDS ARROW (next, flow)
↓ U+2193 DOWNWARDS ARROW (more below)
```

**Currency:**
```
¤ U+00A4 CURRENCY SIGN
£ U+00A3 POUND (use ¤ instead for universal)
€ U+20AC EURO (use ¤ instead)
```

---

### Appendix B: Default Key Mappings Recap

**Left Grid (4 rows, 3 columns + spacebar):**
```
Row 1: CAR    CDR    CONS
Row 2: NIL    EVAL   QUOTE
Row 3: LAMBDA APPLY  ATOM
Row 4: EQ     BACK   INFO
                         (EVAL extends as 1.75U bar)
```

**Right Grid (4×4 phone-layout numpad + operator column):**
```
Col 1-3: Numpad (0–9, ., ENT) — phone layout (ADR-0016 §5)
Col 4:   /, *, -, +

Row 1: 1 2 3 /
Row 2: 4 5 6 *
Row 3: 7 8 9 -
Row 4: . 0 ENT +
```

See `docs/software/runtime/input-dispatch.md` §1 for the canonical data grid layout.

---

### Appendix C: Common Cartridge Customization Checklist

When designing a new cartridge module, ensure:

**UI Screens:**
- [ ] Mission board integration (use Pattern 1 with standard columns)
- [ ] Contract inspector (Pattern 5, cartridge-specific fields)
- [ ] Active phase screen (choose 1–2 primary patterns for gameplay)
- [ ] Debrief screen (cartridge payload, standard nOSh frame)

**Audio:**
- [ ] Voice 1: Navigation (use shared standard tones)
- [ ] Voice 2: Gameplay events (define 5–8 unique tones per module)
- [ ] Voice 3: Ambient/tension (define looping or event-triggered)

**Navigation Grammar:**
- [ ] CAR behavior (always "enter" or equivalent)
- [ ] CDR behavior (always "next" or "traverse")
- [ ] BACK behavior (always "return" or "exit")
- [ ] Special keys (APPLY, QUOTE, CONS if used)
- [ ] No reassigned semantics (CAR must not mean "cancel")

**Data Structures:**
- [ ] Custom cell types defined (if using capability model)
- [ ] Deck state integration (which SRAM fields are written)
- [ ] Cartridge history bit assigned (for cross-module detection)
- [ ] Save data size estimated (per-cartridge save area)

**Multi-Phase Support:**
- [ ] Phase template structure defined
- [ ] Cross-module phase compatibility documented
- [ ] Hot Swap modal integration tested

---

## Stdlib Display Helpers

Cart authors have three stdlib primitives for the most common drawing tasks. Use these instead of hand-rolling with `nosh_text_putc` — they enforce range validation (ADR-0005) and use consistent CP437 glyphs across all carts.

All three are available as C macros (`draw_threat_bar`, `draw_progress_bar`, `draw_bordered_box`) and as Lisp primitives (`draw-threat-bar`, `draw-progress-bar`, `draw-bordered-box`). Return codes (C) and nil/t (Lisp) signal range violations.

### `draw-threat-bar` / `draw_threat_bar`

```scheme
(draw-threat-bar level max-level col row)
```

Renders `level` filled squares (■, KN86_SYM_SQUARE 0x07) followed by `(max-level - level)` open squares (□, KN86_SYM_OSQUARE 0x08) at `(col, row)`. `max-level` must be 1–16. `level > max-level` is silently clamped. Returns `nil` if `col + max-level > 80`.

```scheme
; Example: 5/8 threat, column 2, row 5
(draw-threat-bar 5 8 2 5)
; Renders: ■■■■■□□□ starting at col 2
```

### `draw-progress-bar` / `draw_progress_bar`

```scheme
(draw-progress-bar pct col row width)
```

Renders a `width`-character progress bar `[ XXXX···· ]` at `(col, row)`. Interior uses KN86_BLOCK_FULL (0xA2, █) for filled cells and KN86_SYM_BULLET (0x05, •) for empty cells, giving visible texture even at 0%. `width` must be 4–32. `pct` must be 0–100.

```scheme
; Example: 75% complete, 24-char bar at col 2, row 6
(draw-progress-bar 75 2 6 24)
; Renders: [████████████████····] (22-char interior, 17 filled)
```

### `draw-bordered-box` / `draw_bordered_box`

```scheme
(draw-bordered-box col row w h title)
```

Draws a single-line box of dimensions `w × h` at `(col, row)`, using CP437 corners (┌┐└┘), horizontal (─), and vertical (│) edges. Interior cells are untouched — the cart draws content inside after this call. `title` (optional, pass `nil` for no title) is centered in the top edge with one space of padding on each side; long titles are truncated to `w-4` chars with `..` suffix.

The helper does NOT enforce the row-0/24 firmware-authority split — that is the calling cart's responsibility.

```scheme
; Example: 40×12 panel at col 0, row 5 with title
(draw-bordered-box 0 5 40 12 "ICE LATTICE")

; Small alert box — no title
(draw-bordered-box 20 8 40 8 nil)
```

**Rule:** `w >= 3`, `h >= 2`. `col + w <= 80`, `row + h <= 25`.

---

**End of Document**

This design system is a living reference. As new cartridges are designed and added to the KN-86 library, update this document to incorporate new patterns, audio conventions, and interaction paradigms that achieve consistency across the platform.
