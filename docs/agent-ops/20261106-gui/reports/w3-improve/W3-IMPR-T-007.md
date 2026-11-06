<!-- ORCH-DIGEST:START -->
agent: W3-IMPR | scope: wave 3 / T-007 MAJOR R-W3-002-001 (Tick thread-safety overclaim) | status: DONE
findings: FIXED 1 | REJECTED 0 | DEFERRED 3 (R-W3-002-002/003/004 are MINOR/NIT, no action needed or out of scope)
blockers_major_open: none
code_commits: b0cfe38 | report_commit: pending
tests_added_or_strengthened: 0 (comment-only fix; existing regression test re-verified)
ownership_violations: none
confidence: high (finding evidence re-verified against current Tick() body; scoped tests re-run)
<!-- ORCH-DIGEST:END -->

# Improvement report — W3-IMPR (T-007)

## 1. Finding-by-finding disposition
| Finding | Severity | Disposition | What I did / why rejected or deferred (with evidence) | Commit |
|---|---|---|---|---|
| R-W3-002-001 Header declares Tick multi-thread-safe; only the streaming commit is serialized | MAJOR | FIXED (doc-narrowing, as recommended) | Rewrote the Tick contract comment in DictationEngine.h:44-53 to state exactly what commit_mutex_ guarantees: the streaming-segment commit inside Tick is serialized and safe under concurrent Tick callers (exactly-once per [committed, silence_start) segment); the remainder of Tick (HandleMicLost, hands-free/max-recording timeouts, ARMED-timeout FSM triggers) is single-caller-only, as in production. Also noted TakeTail() reads committed_samples_ without commit_mutex_ (R-W3-002-002, pre-existing) in the same contract, and added a matching scope sentence to the DECISION comment in DictationEngine.cpp:224-233. No code lines changed — comments only. | b0cfe38 |
| R-W3-002-002 commit_mutex_ does not cover TakeTail/StopAndTranscribe tail read | MINOR | DEFERRED / documented | Pre-existing, latent while production is single-worker; reviewer recommends tracking as a known limitation, not fixing in ENG-B1 scope. I documented it in the Tick contract comment (one sentence) so the limitation is visible at the public contract. No behavior change. | b0cfe38 |
| R-W3-002-003 "CommitSegment cannot throw" inaccurate | MINOR | N/A (report wording, not code) | No code change needed per reviewer; noted here for the record. The new comment does not repeat the "cannot throw" wording. | — |
| R-W3-002-004 Fail-before is empirical on single-core runners | NIT | N/A | Disclosed by implementer and accepted by reviewer; optional hardening explicitly not recommended now. No action. | — |

## 2. Code changes (per file, with rationale)
- `src/core/DictationEngine.h` (comments only): the old contract "Tick is safe to call from more than one thread ... the class does not rely on that" is false as stated — `Tick()` [VERIFIED src/core/DictationEngine.cpp:567-597] also runs `HandleMicLost()` (573-575), hands-free timeout -> `StopAndTranscribe` (581-586), max-recording timeout -> `StopAndTranscribe` (587-590), and ARMED timeout FSM trigger (592-595), none serialized; two concurrent Ticks observing the same timeout would both enter `StopAndTranscribe`. New contract narrows the claim to the commit-only guarantee and names the single-caller remainder + TakeTail gap. Root-cause fix at doc level: the public contract is what future callers rely on, so it now matches the actual invariant ("one [committed, silence_start) range commits at most once", enforced under commit_mutex_).
- `src/core/DictationEngine.cpp` (comments only): added a scope sentence to the existing DECISION comment in `MaybeCommitStreamingSegment` so a reader of the lock site sees immediately that only the commit is serialized and that Tick's timeout paths and `TakeTail()` remain single-caller-only (cross-references the Tick contract). This prevents the next reader from re-generalizing the guarantee.
- No behavior change: no signatures, branches, locks, or call order touched. Serializing all of Tick was considered and rejected (reviewer's recommendation too): larger blast radius than ENG-B1's scope, and safety of the single production caller is unchanged.

## 3. Test changes
None. This is a comment-only fix; behavior is byte-identical. I re-ran the ENG-B1 regression test and the streaming subset to confirm the documented guarantee still holds (see §5). No new test is meaningful for a doc fix; the pre-existing barrier test (`concurrent Tick callers commit a streaming segment exactly once`) remains the guard for the commit guarantee.

## 4. Optimizations applied and evidence of benefit
None (doc-only; declined per brief). Reviewer optimization items #1 and #3 (copy-under-lock, barrier style) were explicitly NOT-WORTH-IT for ENG-B1; not touched.

## 5. Commands run and results (scoped)
- [RAN cmake --build build -j 4 -> [100%] Built target local-wisper-flow (and lwf_tests); first invocation printed exit code 1 through a `Select-Object` pipeline, re-run without that pipe completed cleanly — build itself succeeded both times]
- [RAN .\build\lwf_tests.exe --test-case="*streaming*" -> 4 passed / 0 failed, 15 assertions]
- [RAN .\build\lwf_tests.exe --test-case="concurrent Tick callers commit a streaming segment exactly once" -> 1 passed / 0 failed, 5 assertions; repeated 5× -> 5/5 SUCCESS]
- Full 225-case suite NOT re-run (scoped-check rule; comment-only change cannot affect it, and the streaming subset + regression test green is sufficient evidence).
- Hygiene script NOT re-run (no OS-header/network paths touched; comment-only).

## 6. Residual risks and anything the delta reviewer should check
- The narrow contract now says concurrent Tick is safe for the commit, but two concurrent Ticks can still both run `StopAndTranscribe` — that hazard is pre-existing and now documented, but a future caller who ignores the comment could still hit it. Serializing all of Tick (or adding an internal Tick guard) remains the only full fix and was deliberately out of scope; recommend tracking as a separate task if multi-threaded Tick ever becomes a real requirement.
- `chunk_count_`/`speech_chunk_count_` are read under commit_mutex_ in MaybeCommitStreamingSegment but written from the audio path (OnAudioChunk); [ASSUMED pre-existing, not touched] — delta reviewer may wish to confirm this was pre-existing and is outside ENG-B1's write-set. Not introduced or changed by this commit.
- Diff verified: only the two write-set files changed (+12/-5, comments only) [RAN git diff --stat]; no src/ui/src/os/tests touched.
- COMMS.md: read at start; appended DONE line after report commit (left uncommitted per protocol — only ORCH commits COMMS.md). Last line read before finishing: `docs/agent-ops/20261106-gui/COMMS.md` line 46 (W3-IMPL-B1 DONE T-007).
