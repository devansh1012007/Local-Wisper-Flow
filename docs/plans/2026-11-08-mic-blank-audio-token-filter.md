# Plan — token-aware [BLANK_AUDIO] filter (mic-fix, 2026-11-08)

## Facts
- [VERIFIED src/core/WhisperText.cpp:62-79] Filter is whole-string equality only.
- [VERIFIED src/core/StreamingDictation.cpp:127-140] JoinTexts joins segments with a single space; >=2 hallucinated segments yield "[BLANK_AUDIO] [BLANK_AUDIO]" which evades the filter.
- [VERIFIED src/core/DictationEngine.cpp:593] Existing filter sits after both transcription sources and before command branch and pipeline (57fef54).
- [VERIFIED src/core/LocalWhisperProvider? src/stt/LocalWhisperProvider.cpp:109-116] Segments concatenated with no added separator, so "hello [BLANK_AUDIO]" / glued "hello[BLANK_AUDIO]" can also evade.
- [VERIFIED tests/whisperfilter_test.cpp:23-24] "the song had (music) in the title" must survive unchanged; quoted material must be preserved.

## Approach — token-aware FilterHallucinations (chosen)
1. Square-bracket markers ([blank_audio], [silence], [inaudible], [noise], [music], case-insensitive, trailing punctuation tolerated) are removed anywhere — token-level and as substrings (glued segments), replaced by a space, then punctuation-only tokens dropped.
2. Parenthetical markers and "...": removed when they make up the whole transcript or sit at the boundaries (leading/trailing) or next to other markers; preserved when surrounded by real words on both sides (satisfies the (music) in-title test and quoted "(music)" material — quoted tokens are not exact matches and are preserved).
3. IsOnlyHallucination == Filter result is empty (consistent by construction).
4. Rejoin surviving tokens with a single space (TextPipeline normalizes whitespace later).
5. Defensive re-filter: full Filter before InjectText in DictationEngine (post-pipeline, LLM could reintroduce markers), IsOnly guard in CommandModeController (LLM output that is marker-only is unusable, same as empty), Filter before InjectText in HandlePasteLast (historically polluted last_text_).

## Alternatives rejected
- Builder-2: strip every parenthetical anywhere — would break the in-title test.
- Provider-level filtering (LocalWhisperProvider/SttRouter): leaks cloud/streaming/command paths.
- Engine-only fix with unchanged WhisperText: double markers still bypass.

## Blast radius
- Only transcripts containing marker tokens change; real speech is trim-only (single-space join). Marker-only transcripts change from injected token to no injection.
- Rollback: revert WhisperText.cpp + 3 injector guards.

## Status: done (a610d0a + 083d3a2, fix/mic-blank-audio)
- Review: APPROVE (no critical/major; minors = accepted tradeoffs: post-pipeline Filter strips marker tokens from user snippet/dictionary expansions, detached punctuation dropped, command-mode mixed markers unguarded by design, end-to-end newline flatten in TextPipeline pre-existing).
- Gates: lwf_tests 533/533 cases, 27840 assertions; hygiene OK; no-network OK; fail-before via stash proven. G3 zig pending.
- Residuals: Windows mic-capture silence (why Whisper hears blank on "hello") is a separate hardware check, NOT fixed here; stale-binary check (verify exe built from new tip) still owed.
