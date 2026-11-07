<!-- ORCH-DIGEST:START -->
agent: W3-IMPL-B1 | task: T-007 | status: DONE
code_commits: 82238341c2627deeef2739a20732d9881b843251 | report_commit: pending
files_changed: 3 (src/core/DictationEngine.h, src/core/DictationEngine.cpp, tests/test_streaming.cpp)
contracts_changed: none (Tick concurrency safety documented, no signature change)
tests: added 1, changed 0; scoped run: pass (225/225, new test 5/5 after fix, 4/4 fail-before with 8 duplicate commits)
ownership_violations: none
deferred_dependencies: none
out_of_scope_findings: 0
temporary_mitigation: no
deep_issue_flag: no
least_confident_about: BeginCapture/Tick interleaving remains out of scope (pre-existing, single-worker in production)
confidence: high (fail-before reproduced empirically 4/4; fix is a single mutex with a proven test)
last_comms_line_read: 2026-11-06T20:15:00Z
<!-- ORCH-DIGEST:END -->

# Implementation report — W3-IMPL-B1 — T-007 ENG-B1 streaming double-commit race

## 1. Task and acceptance criteria (restated)
Fix the DictationEngine::MaybeCommitStreamingSegment double-commit race (check-then-act at src/core/DictationEngine.cpp:213-250) at the source with a mutex following the StreamingDictator pattern; add a deterministic regression test in tests/test_streaming.cpp (concurrent Tick at the commit point, assert StreamedSegments()==1 and TranscribeCount()==2); write-set src/core/DictationEngine.{h,cpp} + tests/test_streaming.cpp only.

## 2. Verification that the problem exists
[VERIFIED src/core/DictationEngine.cpp:213-250] The function loads silence_start/total/committed atomics, checks thresholds, copies GetRecordedAudio, slices [committed, silence_start), stores committed=silence_start and silence_start=-1, then calls CommitSegment — with no lock anywhere in the sequence. [VERIFIED src/core/StreamingDictation.cpp:74-84] CommitSegment is mutex-protected, but that only serializes the queue push; it cannot dedupe a duplicate caller-side slice. [VERIFIED DictationEngine.h:44] Tick() is public. The B1 investigation digest file was not present in the shared tree (glob over docs/agent-ops/20261106-gui found only w0/w1/w2 reports) [VERIFIED glob]; the brief's recommended fix was checked against the code and matches it. Fail-before was reproduced empirically, not just reasoned: see §7.

## 3. Root cause
Check-then-act across atomics: the read of (silence_start, total, committed), the threshold decision, and the mutation (committed=silence_start, silence_start=-1) are not atomic as a group. Any second Tick caller interleaving between the loads and the stores re-derives the identical segment and commits it again — duplicate streamed segment, extra transcription, repeated joined text. Latent in production (Tick only from the single Run() worker [VERIFIED DictationEngine.cpp:79-90]) but real for any other caller.

## 4. Approach chosen and why
Added `std::mutex commit_mutex_` (DictationEngine.h, next to text_mutex_) and hold it for the whole of MaybeCommitStreamingSegment: all loads, checks, the GetRecordedAudio copy, the slice, the state advance, and CommitSegment now happen under the lock, so a second caller re-loads silence_start=-1 (or a fresh, threshold-failing segment) and returns. A lock-free pre-check on silence_start<0 keeps the silent/no-op path lock-free and allocation-free; the full check is repeated under the lock. CommitSegment is a non-blocking enqueue, so holding the lock across it is bounded; the lock is never held across Finish()/StopAndTranscribe(). Lock order is commit_mutex_ -> StreamingDictator::mutex_; Begin/Finish/Cancel never take commit_mutex_, so there is no inversion. Also documented the Tick concurrency contract in the header.
Alternatives rejected: CAS loop on committed_samples_ (still needs silence_start reset + slice atomicity; strictly more complex); serializing all of Tick (broader blast radius, Tick also does hands-free/timeout transitions — out of scope); dedupe inside CommitSegment (wrong layer — identical PCM can be a legitimate repeat).

## 5. What changed (per file)
| File | Change | Why |
|---|---|---|
| src/core/DictationEngine.h | Added `std::mutex commit_mutex_` with rationale comment; documented Tick() multi-thread safety contract above its declaration | Serializes the commit; makes the contract explicit |
| src/core/DictationEngine.cpp | MaybeCommitStreamingSegment: allocation-free pre-check, then `std::lock_guard<std::mutex> lock(commit_mutex_)` wrapping the entire original check-slice-advance-commit body, with a DECISION comment recording lock scope/order | Makes check-and-commit atomic at the source |
| tests/test_streaming.cpp | Added `<atomic>` include and one regression test: "concurrent Tick callers commit a streaming segment exactly once" | Pins the race at the exact commit point |

## 6. Blast radius and contracts
No signatures changed; MaybeCommitStreamingSegment is private with a single caller (Tick, DictationEngine.cpp:565) [VERIFIED grep]. Only behavioral change: concurrent Tick callers can no longer double-commit. Single-threaded production path is byte-for-byte identical logic, plus one uncontended mutex acquisition on commit-eligible calls only. Grep for MaybeCommitStreamingSegment/Tick consumers: only Tick calls it; Tick is called from Run()/ProcessOne() [VERIFIED DictationEngine.cpp:79-101].

## 7. Tests
New test drives the exact long-utterance totals (100 voiced + 26 silent chunks, min_segment 3000ms / pause 700ms) via FeedSamples WITHOUT Tick so the segment is armed but uncommitted, then barrier-releases 8 threads into engine->Tick() simultaneously, asserts StreamedSegments()==1, then finishes the utterance and asserts injected text "hello world hello world" and TranscribeCount()==2.
Fail-before [RAN ./build/lwf_tests.exe --test-case="concurrent Tick..."] on unfixed code, 4/4 runs: StreamedSegments()==8 (all 8 threads committed), TranscribeCount()==9, injected text repeated 8x. The GetRecordedAudio copy between load and store widens the race window enough that all 8 threads interleave deterministically on this machine.
Pass-after [RAN same command]: 5/5 runs pass, 5/5 assertions. Full suite [RAN ctest --test-dir build -R lwf-tests]: 1/1 Passed; full binary [RAN ./build/lwf_tests.exe]: 225/225 cases, 9671/9671 assertions. [RAN bash scripts/check_hygiene.sh]: hygiene OK. [RAN cmake --build build --target local-wisper-flow]: exit 0. No flakiness observed post-fix (barrier + lock make the outcome order-independent).

## 8. Long-run test
Source: the invariant "one [committed,silence_start) range commits at most once" now holds for any number of Tick callers, not just the single production worker. Load: lock is uncontended in production (single caller) — one extra acquire per commit-eligible Tick; no-op path unchanged (pre-check, no lock, no alloc). Hostile: a Tick storm (or future caller) serializes and commits exactly once; no sleep/retry/timeout involved anywhere. Operability: no new failure modes — CommitSegment cannot block or throw into the lock; deadlock excluded by the single lock order. Reversal: removing commit_mutex_ reopens the race and the new test fails immediately (proven by the fail-before runs).

## 9. Edge cases and abuse scenarios
Concurrent Tick storm: exactly one commit, verified. Silence starting/resuming during the lock: re-loads under the lock see fresh values; the original guards (silence_start>total, committed>silence_start, threshold checks, silence_start>buffer size) are all preserved verbatim under the lock. Finish/StopAndTranscribe: commit_mutex_ is not held there, and Finish's CV wait releases StreamingDictator::mutex_, so a Tick blocked on commit_mutex_ during StopAndTranscribe cannot deadlock the streaming worker. Consciously not handled (out of scope): BeginCapture's reset of committed/silence_start and Tick's hands-free/timeout logic remain unsynchronized across concurrent callers — pre-existing, and production calls them only from the Run worker; ENG-B1 covers the commit only.

## 10. Performance and security considerations
No new allocations on the no-op path (pre-check returns before the lock and before GetRecordedAudio). Commit path: same O(segment) copy as before, plus one mutex op. No secrets, transcripts, or PII touched.

## 11. Commands run and results (verbatim summaries)
- `cmake --build build --config Release --parallel 4 --target lwf_tests` — builds clean (pre-existing grpc -Wattributes warnings only).
- `./build/lwf_tests.exe --test-case="concurrent Tick callers commit a streaming segment exactly once"` — unfixed: FAIL 4/4 (CHECK 8==1, 9==2); fixed: PASS 5/5.
- `ctest --test-dir build -R lwf-tests --output-on-failure` — 1/1 Passed (16.73s).
- `./build/lwf_tests.exe` (full) — 225/225 cases, 9671/9671 assertions, SUCCESS.
- `bash scripts/check_hygiene.sh` — hygiene: OK.
- `cmake --build build --config Release --parallel 4 --target local-wisper-flow` — exit 0, Built target.
- `git diff --stat` — exactly the 3 write-set files, +102 lines.

## 12. Deviations from the plan/brief; ownership issues; deferred dependencies
None. The B1 digest file referenced in the brief was absent from the shared tree; proceeded from the brief + direct code verification (documented in §2). Only write-set files were touched; the two pre-existing untracked files in git status (W1-IMPL-A2 brief, w2-review reports) belong to other agents and were left alone.

## 13. Out-of-scope findings
None.

## 14. Known limitations, risks, rollback notes
The test's 8-thread barrier makes fail-before empirically deterministic on this machine (4/4), but on a heavily loaded or single-core CI runner the unfixed failure could theoretically be timing-dependent — post-fix the test is order-independent and cannot flake. Rollback: revert the 3 files; the test then fails as documented. Pre-existing concurrent-caller hazards in BeginCapture/Tick (non-commit logic) remain as noted in §9.

## 15. Where I am least confident / what the reviewer should scrutinize
The pre-check-then-lock split: confirm it cannot skip a commit (it only returns when silence_start<0 at that instant; any later state change is picked up by the next Tick — same semantics as the original single-threaded code). Also confirm no lock-order inversion with StreamingDictator's destructor path (engine Stop() joins the Run worker before streamer_ is destroyed; no commit_mutex_ is held at destruction).

## 16. Self-review checklist
- [x] Root cause, not symptom (mutex at the check-then-act site, not dedupe in CommitSegment)
- [x] No band-aid patterns (no sleeps/retries/catches; test barrier is harness-side synchronization, not production masking)
- [x] Tests fail without the fix (proven 4/4 unfixed, pass 5/5 fixed; mutation check = removing the lock)
- [x] Only write-set files changed (git diff --stat: 3 files, all in write-set)
- [x] No secrets/PII
- [x] Conventions from AGENTS.md followed (mutex mirroring text_mutex_, DECISION comment style, tests in tests/test_streaming.cpp)

## 17. Commits
code: 82238341c2627deeef2739a20732d9881b843251 | report: pending
