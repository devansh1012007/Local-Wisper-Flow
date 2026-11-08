# Phase 1 — Design + Mockup (for coder brief)
Owner decision: Full unify + Ivory light primary.
Scope: docs/design/mockup.html only (plus records in this file). Do NOT touch src/ui theme headers or tests in Phase 1 — Phase 2 consumes Ph1 token table.
Mockup must: present Ivory Light as primary theme (Full/Mini/Home/Speech/Settings), keep dark as variant section with same struct fields, define new token set for single Palette (bg/surface/border/text/muted/accent/brass/on_accent/ok/warn/err/err_fill/focus/surface_alpha + studio-teal replacements absorbed), type scale, spacing grid, radii, controls >=32px, focus ring 2px, motion rules, state pairing (word+shape+color), contrast tables recomputed via WCAG formula meeting >=4.5 text incl accent/ok/warn/err/brass and >=3 non-text, hairlines exempt <3.0 with rows. Mark Studio teal as deprecated.

## Phase 1 delivery (2026-11-08)
- Mockup: `docs/design/mockup.html` full regen. Ivory Light primary, dark variant, single Palette. All § below mirror the mockup tables.

### Token table (§1 of mockup)
| Field | Ivory primary "Ivory and Brass" | Dark variant "Hunter and Brass" |
|---|---|---|
| bg | 0xF6F1E4 | 0x0E1714 |
| surface | 0xFFFDF7 | 0x14221D |
| border | 0xD9CFBA | 0x29392F |
| text | 0x1C2B26 | 0xEDE6D6 |
| muted | 0x5B665F | 0xA8B3AA |
| accent | 0x7A5C25 (brass) | 0xC9A96E (brass) |
| brass | 0x7A5C25 | 0xC9A96E |
| on_accent | 0xFFFDF7 | 0x0E1714 |
| ok | 0x3F6B52 | 0x8FB39A |
| warn | 0x7A5C25 | 0xD0A65A |
| err | 0x7A2E3A | 0xD98A8F |
| err_fill | 0x7A2E3A | 0xB5565F |
| focus | 0x7A5C25 | 0xC9A96E |
| surface_alpha | 0.90 | 0.88 |

Single-accent invariant: accent == brass == focus in both themes.

### Contrast tables (recomputed verbatim from hex)
Ivory text: text/bg 13.09, text/surface 14.51, muted/bg 5.30, muted/surface 5.88, accent/surface 6.10, accent/bg 5.50, brass/surface 6.10, brass/bg 5.50, ok/surface 6.01, ok/bg 5.42, warn/surface 6.10, warn/bg 5.50, err/surface 9.06, err/bg 8.17, on_accent/accent 6.10 — all PASS ≥4.5.
Ivory non-text: focus/bg 5.50 PASS, focus/surface 6.10 PASS, err_fill/surface 9.06 PASS, err_fill/bg 8.17 PASS; border/bg 1.37 EXEMPT, border/surface 1.52 EXEMPT.
Dark text: text/bg 14.67, text/surface 13.23, muted/bg 8.42, muted/surface 7.59, accent/surface 7.35, accent/bg 8.15, brass/surface 7.35, brass/bg 8.15, ok/surface 7.12, ok/bg 7.89, warn/surface 7.27, warn/bg 8.06, err/surface 6.25, err/bg 6.93, on_accent/accent 8.15 — all PASS ≥4.5.
Dark non-text: focus/bg 8.15 PASS, focus/surface 7.35 PASS, err_fill/surface 3.50 PASS (non-text band), err_fill/bg 3.87 PASS; border/bg 1.49 EXEMPT, border/surface 1.35 EXEMPT.

### Decisions
1. Navy eliminated: light accent/focus 0x1F3A5F → brass #7A5C25 so one accent per view. Former navy (11.29 vs surface) was a second accent; brass 6.10 still passes 4.5 and matches dark Hunter/Brass family.
2. radius_card = 10 single value (native D0 wins over screenshot 12). Studio gap/font twins collapse into metrics values in Phase 2.
3. rail_width = 220 canonical (live "Rail is 220 on purpose" wins over dead token 208). Content max 640, full 960×640 / min 760×520 preserved.
4. Type: title 28, card/section 15, body 14 (up from 13), helper 13, mini 8.5. Phase 2 bumps font_body_px to 14.
5. Motion: functional 150–240 ms band; fade_in standardized 0.12 s → 0.15 s. Ambient (ring 2.4 s, bars 1.2 s, thinking 0.9 s) freezes under reduce_motion with word+dot fallback.
6. err_fill is fill-only (non-text). Dark 3.50/3.87 is in the ≥3 band by design; error copy uses err (6.25). Mockup states this explicitly so Phase 2 tests never assert err_fill ≥4.5 as text.
7. Heat ramp §4B: 0=border, 1=brass 20%, 2=45%, 3=70%, 4=brass100% over surface. Ivory: #D9CFBA/#E4DDCD/#C3B599/#A28C64/#7A5C25; Dark: #29392F/#383D2D/#655F41/#938056/#C9A96E. Teal #CDEBE7/#7DBFB7/#2F8077/#1E5E5A fully retired; L1–L2 tints are supplementary to numeric labels (same contract as teal).
8. Fonts: EB Garamond/Inter remain proposal pending bundling; scaled default font stays. No FreeType/new font files in Phase 1.

### Appendix — contrast spot-checks (python, WCAG formula)
- text #1C2B26 on bg #F6F1E4 = 13.09 (mockup 13.09 match)
- accent #7A5C25 on surface #FFFDF7 = 6.10 (mockup 6.10 match)
- err_fill #B5565F on surface #14221D = 3.50 (mockup 3.50 match)
- brass #7A5C25 on bg #F6F1E4 = 5.50 (mockup 5.50 match)
- on_accent #FFFDF7 on accent #7A5C25 = 6.10 (mockup 6.10 match)
