# Plan — [BLANK_AUDIO] hallucination filter wiring (2026-11-08)

## Problem
Saying "hello" on mic injects literal `[BLANK_AUDIO]`. `WhisperText::FilterHallucinations` exists (`WhisperText.cpp:81`, tested) but is never called in production (`grep src/` = def only). Whisper returns the marker verbatim (`LocalWhisperProvider.cpp:109-117`), `DictationEngine::StopAndTranscribe` passes it straight to pipeline/inject (`DictationEngine.cpp:574→613→628`). LLM cleanup cannot be the cause: `min_words_=4` (`DictationCleanup.h:31`) means 1-word input returns before `llm_->Process` (`DictationCleanup.cpp:184-189`) — the LLM is never even invoked for "hello".

## Goal / acceptance
- Marker-only transcripts (`[BLANK_AUDIO]`, `(music)`, etc.) produce NO injection; engine hits the existing `final_text.empty()` guard (`DictationEngine.cpp:621`) → "nothing to inject".
- Real speech unchanged (trim-only path in `FilterHallucinations`); quotes inside real speech preserved (already tested).
- Covers one-shot + streaming (same choke point) + command-mode branch (it consumes `raw` at 598, before the 613 empty check).

## Approaches
- **A. Filter in `DictationEngine::StopAndTranscribe` right after `raw` is finalized (~line 581, before command branch).** Single seam covering one-shot, streaming (563-564), and command-mode. `raw_text` stats (646) still records the marker — acceptable (it records what STT said). Chosen.
- **B. Filter at `TextPipeline::ProcessWithStats` entry (TextPipeline.cpp:75).** Reusable but leaves command-mode leaking; rejected as primary (would need a second site).
- **C. Filter inside `LocalWhisperProvider::RunWhisper`.** Leaks for cloud STT (SttRouter returns cloud verbatim) and streaming callers pass prompts; rejected.

## Blast radius / rollback
- Only marker-only transcripts change behavior (empty instead of injected marker). All other text trim-only. Rollback: revert one line + include.
- Underlying audio question remains: why is captured audio blank when "hello" is spoken (mic device/level/VAD)? That is a separate hardware check — the filter ensures blank audio yields *no output*, not the literal token. Manual mic-level check still owed on Windows.

## Tasks
- T-002 single atomic task: include + one filter line in DictationEngine, doctest coverage (engine: marker→no inject, streaming marker→no inject, command marker→rejected, "hello world" preserved, LLM not called on 1-word). No dependencies. Done criteria: gates green + independent review.
- Status: plan
