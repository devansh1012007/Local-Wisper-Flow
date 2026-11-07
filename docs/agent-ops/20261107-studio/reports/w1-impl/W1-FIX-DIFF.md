# W1-FIX-DIFF — TextDiff NBSP test hex-escape fix (T-003)

## ORCH-DIGEST
Status: COMPLETE. Root cause is the test literal, not src/core/TextDiff.{h,cpp}: at tests/test_textdiff.cpp:110 `"a\xC2\xA0b"` — the `\xA0` hex escape greedily consumes the following `b` (a hex digit), so the compiler forms `\xA0b` = 0xA0B truncated to one byte (0x0B, vertical tab). The intended UTF-8 NBSP (U+00A0, bytes C2 A0) is never produced; instead the input is `a C2 0B`, which Tokenize splits as `a\xC2` + separator, giving 0 Equal / 1 Delete / 2 Insert → replaced=1, inserted=1 — exactly the observed failure (CHECK replaced==0 got 1, inserted==0 got 1, Equal==2 got 0). TextDiff's UTF-8 decoder correctly decodes C2 A0 to U+00A0 and IsSpaceCodePoint(0xA0) is true, so the tie-break (Insert-first backtrack, TextDiff.cpp:108 `>` — tie falls to Insert) and LCS counting were never at fault; 02A's verifier was right. Fix: split the literal `"a\xC2\xA0" "b"` (adjacent string-literal concatenation ends the hex escape before `b`). One line in tests/test_textdiff.cpp only; no assertion weakened, no TextDiff change. Verification: filtered textdiff cases 11/11 pass (48 assertions); full lwf_tests.exe 306/308 pass, remaining 2 are out-of-write-set dictionarystore cases (test_dictionarystore.cpp:139,174), untouched.

## Context Report (pre-change investigation)
- Reproduction [RAN `cmake --build build --config Release --parallel 4 --target lwf_tests` → exit 0; `ctest --test-dir build --output-on-failure` → lwf-tests failed with test_textdiff.cpp:109 "non-breaking space separates tokens": replaced 1==0, inserted 1==0, Equal 0==2]. Other failures in that run (dictionarystore, errors/TransformStore, notesstore, privacy, transformstore) are outside this write-set and owned by sibling clusters — not touched.
- Hex-escape evidence [VERIFIED]: C++ standard — a `\x` escape consumes the longest sequence of hex digits. `b` is a hex digit, so `\xA0b` = 0xA0B, truncated to char 0x0B. Bytes actually passed to Diff: `61 C2 0B`. 0x0B is also whitespace (IsSpaceCodePoint 0x09..0x0D), so Tokenize yields one token `a\xC2`, never `a`,`b`. LCS vs `a b` tokens: 0 common → raw inserted 2, raw deleted 1 → replaced=min=1, inserted=1. Matches ctest output exactly.
- Decoder evidence [VERIFIED src/core/TextDiff.cpp:39-56,10-21]: DecodeCodePoint(C2 A0) → cp 0xA0; IsSpaceCodePoint(0xA0) true → Tokenize splits NBSP correctly once the bytes are actually C2 A0. With the test literal garbled, the decoder never sees that sequence. Mutation check: reverting the one-line fix re-breaks exactly this case; no other textdiff case is affected.
- Tie-break [VERIFIED src/core/TextDiff.cpp:108]: `table[i-1][j] > table[i][j-1]` → Delete only on strict greater; tie → Insert. All other textdiff cases (token order, single-word swap, full replace, insert/delete-only) pass, confirming 02A's Insert-first fix stands.

## Phased Changes Executed
- Phase 1 — Test literal fix (tests/test_textdiff.cpp:110): `"a\xC2\xA0b"` → `"a\xC2\xA0" "b"`. Terminates the hex escape at the NBSP bytes so the literal is `a <C2 A0> b` = `a`, NBSP, `b` as intended.
- No changes to src/core/TextDiff.h, src/core/TextDiff.cpp, and no other test lines. No assertions added, removed, or weakened.

## Review Audit
- Self-review: `git diff -- tests/test_textdiff.cpp` is exactly one changed line. No production code changed, so no behavior change for any other caller of Diff(). The `a\xC2\xA0" "b"` form also silences the hex-escape warning flagged by 02A at :110.
- Write-set check: modified only tests/test_textdiff.cpp (plus this report). Pre-existing sibling state (M COMMS.md, ?? briefs/) left unstaged. CRITICAL/MAJOR: none found.
- Long-run test: Source — hex-escape bug class recurs wherever `\x..` is followed by a hex-digit char; concatenation is the standard idiom. Load — Tokenize/LCS unchanged, O(n*m) as before. Hostile — malformed UTF-8 still degrades to single-byte tokens (DecodeCodePoint fallbacks), unaffected. Operability — no new warnings. Reversal — revert the one line, test re-fails deterministically.

## Verification Evidence
- [RAN `cmake --build build --config Release --parallel 4 --target lwf_tests` → exit 0: test_textdiff.cpp recompiled, 100% Built target lwf_tests]
- [RAN `.\build\lwf_tests.exe --test-case="*textdiff*,*non-breaking*,*whitespace*,*token order*,*empty and empty*,*identical text*,*insertion only*,*deletion only*,*fully replaced*,*single word swapped*"` → 11/11 passed, 48/48 assertions, Status SUCCESS]
- [RAN broader filter `*diff*,*token*,*whitespace*,*non-breaking*,*identical*,*insertion*,*deletion*,*replaced*,*swapped*,*empty and empty*` → 17/17 passed, 109/109 assertions]
- [RAN full `.\build\lwf_tests.exe` → 308 cases, 306 passed, 2 failed (test_dictionarystore.cpp:139,174 — FORBIDDEN, other owner), 12399/12402 assertions. Zero textdiff failures; delta vs baseline 300/308 is the 1 textdiff recovery plus sibling-cluster recoveries outside this fix.]
- NOTE: full `ctest --test-dir build` was not re-run to completion in this fix (it exceeds 300s timeout in this env due to unrelated re2 tests); the full lwf_tests binary run above is the equivalent doctest evidence.

## Unverified Assumptions & Remaining Risks
- [ASSUMED] Source file encoding is UTF-8/ASCII-compatible for the hex-escape form (no raw NBSP char introduced) — the fix uses only ASCII escapes, so no encoding assumption is added.
- UNKNOWN: none in this cluster. Remaining red (dictionarystore ×2) belongs to other owners.

## Git State
Branch: feature/studio-pages. This agent's files (explicit-path commits only): tests/test_textdiff.cpp, docs/agent-ops/20261107-studio/reports/w1-impl/W1-FIX-DIFF.md. Sibling state left unstaged. Commits: fix commit + report commit, trailers Task: T-003 / Agent: W1-FIX-DIFF / Wave: 1.
