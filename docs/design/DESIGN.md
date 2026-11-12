# Hairline — Local Wisper Flow design specification

Status: **SIGNED OFF — accent A lime/signal (2026-11-09)**  
Branch: `ops/ui-redesign-v2`  
Date: 2026-11-09  
Plan: `C:\Users\devan\.opencode\plan\ui-redesign-v2-draft.md` (approved plan; Design direction, AC-G1..G6, feature map, Task 1)  
Gate: owner sign-off completed 2026-11-09 on accent choice (§4) and open decisions (§12). **T2 (token swap) is unblocked.** No source code changes are made by this task.

This file is the source of truth for the Hairline identity on this branch. `docs/design/mockup.html` is superseded by this file on `ops/ui-redesign-v2` (retained for history; see its legacy banner). Nothing in this file copies or restates the legacy mockup token values.

## 1. Design principles (from approved plan)

1. **No cards, no filled panels.** Structure from 1 px horizontal hairlines, whitespace, and typographic scale. Content reads as ruled ledger rows: label left, control right, helper below.
2. **Navigation is a slim top strip** — text labels, underline for active. Nine destinations: Home, History, Dictionary, Snippets, Transforms, Scratchpad, Insights, Profiles, Settings. See §7 for ordering, overflow, and default size.
3. **Status = 8 px square + word.** The square is the only color carrier; the word is always present. One accent per view: live state, single primary action, focus ring.
4. **Palettes.** Dark "Graphite" is the runtime default; light "Paper" is fully supported via the `theme` switcher (owner decision OQ-A1). Both palettes are contrast-tested (AC-G1). Exact hex in §3; accent candidates per-theme derived in §4.
5. **Type.** Dear ImGui default font only (OQ-A4 default). Hierarchy from size tokens (title 24 / section 14 / body 13 / helper 12 design px) plus muted vs text. Sentence case, verb-first labels (§8).
6. **Motion.** Functional only, 150–240 ms ease-out, measured `dt`, `reduce_motion` honored.
7. **Mini badge.** Dot/pill from hairline + status square, surface-step elevation only.

## 2. Contrast policy (AC-G1)

- All text pairs ≥ 4.5:1.
- Meaningful non-text (focus ring, toggle track, control border, status square) ≥ 3:1.
- Declared hairlines (`hairline` on `bg`, `hairline` on `surface`) are exempt below 3:1 and must never carry state alone (every state is also a word + square/shape).
- `err_fill` is fill-only, never text: asserted at ≥ 3:1, never at 4.5:1 as text.
- Ratios use the WCAG relative-luminance formula: `L = 0.2126R + 0.7152G + 0.0722B` over linearized sRGB channels (`c/12.92` if `c <= 0.03928` else `((c+0.055)/1.055)^2.4`), `ratio = (Lhi+0.05)/(Llo+0.05)`. Reproduce with Appendix A.

## 3. Tokens

All sizes are design px at 1.0 scale; callers multiply by the window DPI scale (tokens × DPI). Colors are `#RRGGBB`.

### 3.1 Dark "Graphite" (provisional — runtime default)

| Token | Value | Role |
|---|---|---|
| bg | #101214 | window background |
| surface | #17191C | raised surface (sheets, modals, inputs fill where needed); elevation is a surface step, never a shadow/filled-panel language |
| hairline | #2A2D31 | 1 px rules (exempt pair) |
| text | #E8E9EA | primary text |
| muted | #9A9DA1 | helper/secondary text |
| accent | §4 per-candidate | the one accent per view (active nav, primary action, focus ring) |
| on_accent | #101214 | text on accent fill (equals bg) |
| ok | #8FD1A0 | Ready/OK status (always + word + square) |
| warn | #E0B35A | restart/attention notes (always + word + square) |
| err | #F08A8F | error copy (never fill) |
| err_fill | #C65A61 | attention dot fill only (non-text) |
| focus | = accent | focus ring, 2 px |
| surface_alpha | 0.88 | Mini badge surface opacity |

### 3.2 Light "Paper" (provisional)

| Token | Value | Role |
|---|---|---|
| bg | #F7F7F5 | window background |
| surface | #FFFFFF | raised surface |
| hairline | #E2E2DF | 1 px rules (exempt pair) |
| text | #1A1A18 | primary text |
| muted | #6B6B66 | helper/secondary text |
| accent | §4 per-candidate | the one accent per view (active nav, primary action, focus ring) |
| on_accent | #FFFFFF | text on accent fill (equals surface) |
| ok | #2F6B45 | Ready/OK status (always + word + square) |
| warn | #7A5500 | restart/attention notes (always + word + square) |
| err | #A61E28 | error copy (never fill) |
| err_fill | #A61E28 | attention dot fill only (non-text; equals err on light) |
| focus | = accent | focus ring, 2 px |
| surface_alpha | 0.90 | Mini badge surface opacity |

### 3.3 Metrics (all × DPI)

| Token | Design px | Notes |
|---|---|---|
| top-strip height | 48 | nav strip incl. status readout; nav hit height ≥ 32 within it |
| hairline width | 1 | rules only; no 2 px strokes except focus ring |
| status square | 8 × 8 | per-state carrier next to the word |
| row height | 52 | settings/ledger rows |
| control min height | 32 | every control ≥ 32 per AC-G2 |
| content max width | 640 | widest page content, left aligned |
| page padding | 32 | outer content inset (vertical) |
| page gutter | 48 | outer content inset (left/right; the top strip aligns to it) |
| gap (between groups) | 24 | |
| gap section | 12 | within group |
| gap inline | 8 | label/control spacing |
| Full default size | 960 × 640 | |
| Full min size | 760 × 520 | overflow rule §7 |

### 3.4 Type scale (default font only, × DPI)

| Token | Design px | Usage |
|---|---|---|
| title | 24 | page title |
| section | 14 | section label |
| body | 13 | row label/control text |
| helper | 12 | helper text (muted) |

No font files are introduced (OQ-A4 default). Hierarchy is size + muted-vs-text only.

## 4. Accent candidates — computed per-theme BEFORE owner pick

Candidates are per-theme derived values (dark value and light value separately; never a single hex for both themes). Steel blue `#5B8FD9` on Paper computes to 3.07:1 (bg) / 3.29:1 (surface) — non-text PASS, text FAIL — confirming that any single-hex steel blue must not ship on light; the light candidate below is a darker derivative.

Each candidate is evaluated on: accent on bg, accent on surface, on_accent on accent (text), plus focus (== accent) non-text. Dark `on_accent` = `#101214`; light `on_accent` = `#FFFFFF`, matching §3.

### Candidate A — lime/signal — **SELECTED (2026-11-09)**

| Pair | Dark: #B8E356 (on #101214) | Light: #3F6212 (on #F7F7F5/#FFFFFF) |
|---|---|---|
| accent on bg | 12.66 PASS (text ≥4.5) | 6.60 PASS (text ≥4.5) |
| accent on surface | 11.88 PASS (text ≥4.5) | 7.08 PASS (text ≥4.5) |
| on_accent on accent | 12.66 PASS (text ≥4.5) | 7.08 PASS (text ≥4.5) |
| focus on bg/surface | 12.66 / 11.88 PASS (non-text ≥3) | 6.60 / 7.08 PASS (non-text ≥3) |

- AC-G1 in each theme: PASS on all measured pairs.
- Inversions fail and prove per-theme derivation: white on dark lime = 1.48 (FAIL); dark on light olive = 2.65 (FAIL).
- Collision note: lime family is adjacent to the ok green (`ok` #8FD1A0 dark / #2F6B45 light). Status is still word + square, never color-alone, but A is the closest candidate to `ok`. Owner to weigh.
- **SELECTED** — owner picked A on 2026-11-09.

### Candidate B — oxide red — **REJECTED**

| Pair | Dark: #E8735A (on #101214) | Light: #9B2C1E (on #F7F7F5/#FFFFFF) |
|---|---|---|
| accent on bg | 6.29 PASS (text ≥4.5) | 7.06 PASS (text ≥4.5) |
| accent on surface | 5.90 PASS (text ≥4.5) | 7.57 PASS (text ≥4.5) |
| on_accent on accent | 6.29 PASS (text ≥4.5) | 7.57 PASS (text ≥4.5) |
| focus on bg/surface | 6.29 / 5.90 PASS (non-text ≥3) | 7.06 / 7.57 PASS (non-text ≥3) |

- AC-G1 in each theme: PASS on all measured pairs.
- Inversions fail: white on dark oxide = 2.99 (FAIL); dark on light oxide = 2.48 (FAIL).
- Collision note (strongest of the three): oxide red is nearly indistinguishable from `err` (#F08A8F dark / #A61E28 light). B would put the one accent on the same hue as the error color. Owner must explicitly accept that adjacency or reject B.
- **REJECTED** — collides with err hue (see collision note above); not chosen by owner (2026-11-09).

### Candidate C — steel blue — **REJECTED**

| Pair | Dark: #7AB3E8 (on #101214) | Light: #2F5D9E (on #F7F7F5/#FFFFFF) |
|---|---|---|
| accent on bg | 8.44 PASS (text ≥4.5) | 6.16 PASS (text ≥4.5) |
| accent on surface | 7.92 PASS (text ≥4.5) | 6.60 PASS (text ≥4.5) |
| on_accent on accent | 8.44 PASS (text ≥4.5) | 6.60 PASS (text ≥4.5) |
| focus on bg/surface | 8.44 / 7.92 PASS (non-text ≥3) | 6.16 / 6.60 PASS (non-text ≥3) |

- AC-G1 in each theme: PASS on all measured pairs (light value is the darker derivative; the naive `#5B8FD9` fails text at 3.07/3.29 as shown above).
- Inversions fail: white on dark steel = 2.22 (FAIL); dark on light steel = 2.84 (FAIL).
- Collision note: steel blue is the most distinct from `ok`/`warn`/`err` of the three candidates.
- **REJECTED** — valid on AC-G1 but not chosen by owner (2026-11-09).

Owner pick: DECIDED — A lime/signal (2026-11-09). T2 is unblocked.

## 5. Contrast — non-accent pairs (computed)

| Pair | Graphite ratio | Verdict | Paper ratio | Verdict |
|---|---|---|---|---|
| text / bg | 15.44 | PASS (text ≥4.5) | 16.25 | PASS |
| text / surface | 14.49 | PASS | 17.43 | PASS |
| muted / bg | 6.89 | PASS | 4.99 | PASS |
| muted / surface | 6.47 | PASS | 5.36 | PASS |
| ok / bg | 10.54 | PASS | 5.91 | PASS |
| ok / surface | 9.89 | PASS | 6.34 | PASS |
| warn / bg | 9.62 | PASS | 6.26 | PASS |
| warn / surface | 9.03 | PASS | 6.72 | PASS |
| err / bg | 7.81 | PASS | 6.90 | PASS |
| err / surface | 7.33 | PASS | 7.40 | PASS |
| err_fill / bg | 4.50 | PASS (non-text ≥3, fill-only) | 6.90 | PASS (non-text ≥3) |
| err_fill / surface | 4.22 | PASS (non-text ≥3, fill-only) | 7.40 | PASS (non-text ≥3) |
| focus ring* | see §4 | PASS (non-text ≥3) | see §4 | PASS (non-text ≥3) |
| hairline / bg | 1.36 | EXEMPT (hairline) | 1.21 | EXEMPT (hairline) |
| hairline / surface | 1.27 | EXEMPT (hairline) | 1.30 | EXEMPT (hairline) |

* `focus` equals `accent` (§3), so the focus-ring non-text ratio equals the candidate's accent-on-bg/surface ratio in §4; every candidate passes ≥3:1 on bg and surface in both themes. No non-accent token pair fails AC-G1.

## 6. Component inventory

No card/filled-panel primitive remains. Every surface is a ruled ledger over `bg`/`surface` with 1 px hairlines.

| Component | Purpose | Anatomy | States | Min size | Keyboard |
|---|---|---|---|---|---|
| Ruled ledger row (label/control/helper) | the single settings/content row | label left, control right, helper below; top hairline | default, disabled, focus | 52 high, control ≥32 high, ≤640 content | Tab order matches visual order; focus ring visible |
| Top-strip nav item | one of 9 destinations | text label + 1 px underline when active | default, hover, active, focus | ≥32 hit in 48 strip | Tab + Enter/Space; `ImGuiButtonFlags_EnableNav`; Ctrl+1..9 |
| Toggle | boolean setting | ledger row with toggle control right | off/on, disabled, focus | ≥32 high | Space toggles; track = accent when on (non-text ≥3) |
| Button — primary | the one primary action per view | accent fill + on_accent text | default, hover, disabled, focus | ≥32 high | Enter/Space; only one per view |
| Button — ghost | secondary action | transparent bg, text border | default, hover, disabled, focus | ≥32 high | Enter/Space |
| Button — destructive | delete/remove | err text, ghost fill | default, armed confirm, disabled, focus | ≥32 high | Enter/Space; two-step named confirm (§8) |
| Status square + word | FSM/consent/locality state | 8 px square + word, never square alone | ready/listening/thinking/paused/attention | square 8, word body 13 | read-only; localized phrase in status line |
| Modal | blocking confirm (delete data, key removal) | sheet + hairline + title/body/ actions | open, focus-trapped | content ≤640 | Tab-trapped; Esc cancels; focus on safe action |
| Sheet | non-blocking editor (snippet/transform) | ruled panel with its own rows | open, dirty, invalid | rows 52 | Tab; Esc closes |
| Toast | transient result | bottom sheet + word, auto-dismiss | info/ok/err | ≥32 high | not focusable (message mirrored in status line) |
| Empty state | zero rows/entries | verb-first line + one action ("Add entry") | empty | — | action reachable by Tab |
| Error state | failed load/save | 3-part copy (§8) in err + square + word | error | — | retry action reachable by Tab |
| Setting row with restart banner | restart-gated key | row + warn square + "Applies after restart" line | clean, pending-restart | row 52 | Tab through row; banner read-only |
| API-key row | masked key entry per service | masked input + status dot/word + save/remove | not set / environment / saved, error | input ≥32 | Tab; Enter saves; buffers never seeded, cleared on commit/switch, never logged |
| Hotkey record row | capture a chord | chip + "Record" / "Clear"; conflict disables save | idle, recording, conflict | ≥32 high | Space records; Esc cancels; modifier required |

**Amendments (T3 as-built).** The table above is pre-T3 intent; T3's shipped widgets record these as-built truths: (a) the active underline is 2 px accent (`TopStripItem`, selected `Chip`) — §6's "1 px underline" is amended and §3.3's "no 2 px strokes except focus ring" exception now also covers the active nav underline; (b) `ToggleSwitch` is a hairline track + 8 px square knob (on = filled accent square, off = hairline square), so state still never carries on color alone — filled square vs hairline square carries it; (c) the ghost/`LightButton` is a border-token outline (1 px `palette.border`), not §6's "text border"; (d) `StatusKind` is a palette-role enum (`Ok`/`Warn`/`Err`/`Accent`/`Muted`), NOT the FSM names listed in the table above (`ready`/`listening`/`thinking`/`paused`/`attention`) — the word carries the identity and the FSM-state-to-role mapping table is deferred to the T4b status line; (e) `Row` draws top AND bottom 1 px hairlines (the table's "top hairline" was shorthand; ledger-correct is ruled on both sides); (f) T4b status line + shortcuts as-built: (d)'s deferred mapping is now shipped — precedence top to bottom is validation error → paused → capturing → thinking → ready, with the locality phrase below. The reserve is `space_xl` × 4 (128 design px) at the strip's right end, vertically centred, square + word + phrase on one line; the phrase is ellipsis-truncated to the reserve at the body font and never spills into Hide. Shortcuts are `Ctrl+1`..`Ctrl+7` mapped positionally to the strip items array (guarded by `WantTextInput`; Full window only; active on Welcome — `first_run` stays true so Welcome returns next launch).

**Followups-2 as-built (2026-11-12, fix/ui-followups-2).** (m) The wordmark reads "Echo flow" (user-visible surfaces only — window title, wordmark, tray, About, crash/log headers; internal identifiers keep the LocalWisperFlow spelling so data dirs and the registry value survive upgrades). (n) The Volume row in Settings > General commits while dragging (IsItemEdited) and posts a throttled PreviewTone sample at the dragged amplitude (0.25 s dwell, final sample on release, gated on sound_enabled). (o) The Mini badge's note icon is removable in place: right-click posts the removal (scratchpad_flow_bar -> false -> ApplySettings drain -> re-hug); the Scratchpad page's flow-bar toggle remains the add path and surfaces the gesture. (p) The shipped Polish / Prompt Engineer transforms are retired: TransformStore seeds ten empty slots (nothing is built_in), Load drops legacy built_in-flagged entries, v1 arrays convert from slot 1, all slots accept Create New / Delete, and the card subtitle always shows the real prompt. The reset modal copy states every slot ends up empty.

**Followups-1 as-built (2026-11-12, ops/ui-followups-1).** (g) The `ToggleSwitch` track is a filled gray (`palette.border` token — 0x2A2D31 Graphite / 0xE2E2DF Paper) plus the 1 px hairline; the square knob stays the state carrier, so amendment (b)'s "hairline track" is superseded. (h) The Mini badge window hugs the icons (`mini_ring_pad` 8 + dot 22 + optional note 16 + status strip 18 = 38×48 at 1.0 scale, no note); the Attention state expands the window to the 250 px message reserve and shrinks back on leaving Attention. (i) The Transforms and Snippets page-top banners (and the Transforms opted-out notice strip) are retired; the header Opt-in toggle and the How-it-works modal remain. (j) The Scratchpad hero banner is retired; "Start new note" lives in the Recents toolbar. (k) The Welcome 4-step wizard is retired (intro + push-to-talk chord + model pick + Get started). (l) The full window's page gutter is 48 px left/right (strip aligned); vertical padding stays 32.

T4b FSM-to-`StatusKind` mapping (as-built, top row wins):

| Condition (top match wins) | `StatusKind` | Word | Locality phrase |
|---|---|---|---|
| `validation_error_` non-empty | `Err` | Attention | locality (below) |
| `dictation_enabled` is false | `Muted` | Paused | locality (below) |
| `CAPTURING` | `Accent` | Listening | locality (below) |
| `TRANSCRIBING` / `POSTPROCESSING` / `EMITTING` | `Accent` | Thinking | locality (below) |
| `ARMED` / `IDLE` | `Ok` | Ready | locality (below) |

Locality phrase (cloud wins over cleanup — the destination of spoken audio is the bigger privacy fact):

| Condition | Phrase |
|---|---|
| `stt_consent_accepted` and `stt_provider` is `gnani` | Cloud: Gnani |
| `stt_consent_accepted` and `stt_provider` is `elevenlabs` | Cloud: ElevenLabs |
| `stt_consent_accepted` and `stt_provider` is `muse` | Cloud: Muse |
| `stt_consent_accepted` and any other/unknown value | Cloud |
| otherwise, `cleanup_enabled` and `llm_provider` is `openrouter` | OpenRouter |
| otherwise | Local |

Reads are direct `fsm_.CurrentState()` + `settings_` (Home pattern, no snapshot — by design, since `MainWindow` reads zero snapshots today). `StatusSquare` is called with `*current_palette_` explicitly: its default is `kLightPalette` (Paper) and would break Graphite. Error state comes only from `validation_error_` (FSM-only error source — no snapshot read, by design).

**Hover/active wash exception (T3).** Interactive hover/active washes are the sanctioned exception to "no fill": `LightButton` uses a border-token wash at 15% alpha on hover / 25% on active (never a surface fill); `OutlinePill`'s active border is opaque (pre-existing, kept).

## 7. IA map — every feature maps to exactly one destination

Top strip (left to right): Home, History, Dictionary, Snippets, Transforms, Scratchpad, Insights, Profiles, Settings. Ctrl+1..9 map in this order. Default window 960×640; minimum 760×520. Overflow rule at 760×520 (and 200% DPI): labels never wrap; the strip keeps all 9 entries reachable — labels truncate with ellipsis from the end first (Scratchpad, Insights, Profiles, Settings last), then the strip scrolls horizontally with a visible edge fade; no destination is ever moved to a second row or menu. T4 must verify mouse + keyboard at 960×640 and 760×520 × 100/150/200% DPI.

Additional surfaces: Settings tabs General / Speech / Cloud / Cleanup / Privacy / About; Welcome (intro + push-to-talk chord + model pick + Get started; only before first run / empty model path — the 4-step wizard was retired in followups-1); Mini badge (separate OS window, never focusable, never shows transcripts; hugs the icons, expands only for the attention message); ScratchpadWindow (separate OS window, third ImGui context); tray menu (owns Quit; Open Settings…, Open History, Dictation on/off, Autostart, Quit — tray mirrors that write settings route through the same command path and are not second owners; see owner column below).

"Owner" below is the single edit destination for the key. Read-only derivations (Home trust line, tray mirrors, Welcome repeats, About read-only display) are mirrors, not owners, and are labeled as such. Restart-gated (`hot_apply=false`) = 8 keys: whisper_model_path, llm_model_path, mic_device, llm_provider, openrouter_model + carve-outs schema_version, mini_x, mini_y. Carve-outs (no Settings edit row) = 6 keys: schema_version, first_run, last_page, mini_x, mini_y, hotkey_code. `hotkey_code` is the 6th no-row key (schema-retained, its UI row removed in T14) — listed once below as that destination.

| Key / feature | Owner destination | Notes |
|---|---|---|
| schema_version | Settings > About (read-only display) | carve-out: not an edit control |
| hotkey_code | Settings > General (row removed) | schema key stays; UI row deleted in T14 |
| hotkey_ptt | Settings > General | Home/Welcome show the chord (mirror) + record in General |
| hotkey_handsfree | Settings > General | |
| hotkey_cancel | Settings > General | |
| hotkey_command | Settings > General | |
| hotkey_paste_last | Settings > General | |
| double_tap_handsfree | Home (quick action) | |
| double_tap_window_ms | Settings > Speech | pairs with double-tap behavior |
| min_hold_ms | Settings > Speech | |
| max_recording_ms | Settings > Speech | |
| hands_free_silence_ms | Settings > Speech | |
| avoid_altgr_conflict | Settings > Speech | |
| whisper_model_path | Settings > Speech (+ Welcome step 3) | restart-gated; download flow (T13a) on both surfaces |
| whisper_language | Settings > Speech | |
| vad_threshold | Settings > Speech | |
| llm_model_path | Settings > Speech | restart-gated |
| cleanup_enabled | Settings > Cleanup | Home trust line derives from it (mirror) |
| cleanup_min_words | Settings > Cleanup | |
| cleanup_timeout_ms | Settings > Cleanup | |
| cleanup_language_policy | Settings > Cleanup | |
| spoken_commands_aggressive | Settings > Cleanup | |
| dictionary_fuzzy | Settings > Cleanup | |
| snippet_mode | Settings > Cleanup | |
| command_timeout_ms | Settings > Cleanup | |
| use_remote | Settings > Cloud | |
| remote_endpoint | Settings > Cloud | |
| inject_delay_ms | Settings > Cleanup | |
| streaming_stt | Settings > Speech | |
| streaming_min_segment_ms | Settings > Speech | |
| streaming_pause_ms | Settings > Speech | |
| stt_provider | Settings > Cloud | Home trust line derives from it (mirror) |
| stt_consent_accepted | Settings > Cloud | consent surface; Home trust derives (mirror) |
| stt_language | Settings > Cloud | |
| stt_timeout_ms | Settings > Cloud | |
| stt_max_clips | Settings > Cloud | |
| mic_device | Settings > Speech | restart-gated; picker lists System default + capture devices |
| sound_enabled | Settings > General | |
| sound_volume | Settings > General | |
| reduce_motion | Settings > General | motion tokens still honored at 150–240 ms |
| mini_x | Mini badge (written by drag) | carve-out: no Settings row |
| mini_y | Mini badge (written by drag) | carve-out: no Settings row |
| history_enabled | Settings > Privacy | |
| keep_raw_in_history | Settings > Privacy | |
| history_max_entries | Settings > Privacy | |
| history_max_age_days | Settings > Privacy | |
| debug_log_transcripts | Settings > Privacy | requires explicit confirmation; default off |
| dictation_enabled | Home (quick action + tray mirror) | |
| first_run | Welcome | carve-out: set false on Welcome finish; no Settings row |
| scratchpad_flow_bar | Settings > General | controls the Mini scratchpad bar |
| keep_usage_stats | Settings > Privacy | Insights reads it (mirror) |
| last_page | Full window shell (persisted on navigation) | carve-out: no Settings row |
| quiet_speech_boost | Settings > Speech | |
| llm_provider | Settings > Cleanup | restart-gated; Privacy/About derive (mirror) |
| openrouter_model | Settings > Cleanup | restart-gated; picker + manual id fallback |
| theme (new key, default graphite) | Settings > General | theme switcher row (OQ-A1, T2b/T5) |

Feature map (non-key): error states → the page that failed + status line (3-part copy §8); History page → History (new; tray History… routes here; retention link → Settings > Privacy; export §11); model download → Welcome step 3 + Settings > Speech (same engine, T13a); app profiles → Profiles (new; viewer + assign depth §12); autostart → tray toggle (wired in T14; no Settings row); transform slots → Transforms (per-slot Run posts its real slot, T10c); dead stubs REMOVED (not destinations): Dictionary "Shared with team" tab, Insights "Your voice" empty tab, Insights Share "Coming soon" stubs — removed, no redirect.

## 8. Copy rules

Sentence case. Verb-first labels and buttons ("Save key", "Remove saved key", "Record hotkey", "Refresh model list"). No exclamation marks.

Banned whole-words in display strings: oops, fsm, vad, llm, whisper, provider, providers, pipeline, pipelines, stt. Use plain words instead (e.g. "speech to text" spelled out, never the abbreviation; "cleanup" spelled out where the target reads "cleanup" as the word — config keys and code identifiers are not display strings and are exempt from this lint).

Error copy is always 3 parts: (1) what happened, (2) why, (3) what to do next. Example shape: "Model download did not finish. The connection dropped before all bytes arrived. Check your connection and choose Retry." Never log transcripts or keys; key buffers are masked, never seeded, cleared on commit/switch; attempt errors live in redacted `detail` only.

## 9. Non-goals

Deferred to R3: work-area refresh after display changes (D7), Mini dot-drag rework, Mini right-click parity with tray. No engine/audio/STT/LLM/injection/FSM changes. No new dependencies. No font files. No docking/multi-viewport/demo. Schema keys are additive-only (G5): no key is removed, including `hotkey_code` (its UI row only is removed).

## 10. Heat-ramp disposition

DECISION for T2 (recommendation): re-derive `HeatColor` from the chosen accent so the five-stop ramp stays a single-hue family under the winner, instead of keeping the legacy brass ramp as a sanctioned multi-stop exception. Rationale: one accent per view (AC-G2/R9) leaves no room for a second hue family except the status colors (`ok`/`warn`/`err`), which the ramp is not. State stays numeric + legend (never color-alone), so re-deriving does not change the `HeatLevel` math. If the owner prefers the exception, T2 documents the ramp as the sole sanctioned multi-stop exception with its own ≥3:1 assertions on stages 3–4 only (stages 0–2 stay numeric-carried). Default: re-derive.

## 11. History-export mechanism (OQ-A5 default)

Export writes to the data directory and auto-opens the file (default, adopted). A Win32 file picker would require a new `src/os` adapter and is out of scope unless the owner explicitly requests it at sign-off.

## 12. Open decisions (defaults stand unless owner overrides at sign-off)

- Accent: DECIDED — A lime/signal (dark #B8E356 / light #3F6212), signed off 2026-11-09. T2 (token swap) is unblocked.
- T1 signed off 2026-11-09 (accent A); T2 (token swap) is unblocked.
- OQ-A3 — profiles depth: viewer + category assign (default) vs full editor → T12.
- OQ-A4 — fonts: default font only (default) vs approve a TTF.
- OQ-A6 — deferred cuts confirmed: D7 work-area refresh, dot-drag rework, Mini right-click parity → R3.
- OQ-A7 — profile edits: restart-gated (default: persist + "Applies after restart" banner) vs live-reload seam → T12.

---

## Appendix A — contrast computation (reproduce)

```python
def lin(c):
    c = c/255.0
    return c/12.92 if c <= 0.03928 else ((c+0.055)/1.055)**2.4

def lum(h):
    r = int(h[1:3], 16); g = int(h[3:5], 16); b = int(h[5:7], 16)
    return 0.2126*lin(r) + 0.7152*lin(g) + 0.0722*lin(b)

def ratio(a, b):
    hi, lo = max(lum(a), lum(b)), min(lum(a), lum(b))
    return (hi+0.05)/(lo+0.05)
```

Computed outputs (2 dp, verified by running this script):

- Graphite text/bg 15.44, text/surface 14.49, muted/bg 6.89, muted/surface 6.47
- Paper text/bg 16.25, text/surface 17.43, muted/bg 4.99, muted/surface 5.36
- Hairline Graphite bg 1.36, surface 1.27; Paper bg 1.21, surface 1.30 (EXEMPT)
- Naive steel `#5B8FD9` on Paper bg 3.07, surface 3.29 (text FAIL, non-text PASS)
- Graphite ok #8FD1A0 bg 10.54 surface 9.89; warn #E0B35A 9.62/9.03; err #F08A8F 7.81/7.33; err_fill #C65A61 4.50/4.22
- Paper ok #2F6B45 bg 5.91 surface 6.34; warn #7A5500 6.26/6.72; err #A61E28 6.90/7.40
- Candidate A dark #B8E356 12.66/11.88 on #101214; light #3F6212 6.60/7.08 on #F7F7F5/#FFFFFF
- Candidate B dark #E8735A 6.29/5.90; light #9B2C1E 7.06/7.57
- Candidate C dark #7AB3E8 8.44/7.92; light #2F5D9E 6.16/6.60
