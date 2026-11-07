<!-- ORCH-DIGEST:START -->
agent: WCI-FIX | scope: Linux directory-read crash (CI run 42399548181) - regular-file guards at 15 ifstream load sites in 13 files | verdict: COMPLETE
findings: all 15 sites guarded BEFORE open with is_regular_file(ec) matching NotesStore.cpp adoption pattern; missing/corrupt/.bad semantics preserved per site (directory NEVER renamed to .bad); repro test test_notesstore.cpp:341 green; no test expectations changed
claims_audit: pre-fix grep found exactly the briefed sites (tests/test_notesstore.cpp:341 THREW basic_filebuf::underflow Is a directory); post-fix full suite 518 passed / 27740 assertions, ctest 1/1, hygiene OK
band_aids_found: none - root-cause pre-open filesystem type check only; no try/catch broadening, no sleeps, no type bypass, no test edits
top_3_risks: (1) TOCTOU: path swapped between is_regular_file check and open is unchanged class (unavoidable without O_NOFOLLOW-style open); (2) TransformStore directory case enters existing missing-path Seed+Save attempt which fails and warns (by design, same as missing); (3) Linux run itself NOT-RUN on this Windows box - fix is portable C++ verified via repro + full suite on Windows semantics
optimizations: DO-NOW 0 | DO-LATER 0 | NOT-WORTH-IT 0
reversal_check: done (revert = 13 files, 98+/2-; NotesStore repro test fails on Linux pre-fix per CI log)
confidence: high on build/ctest/hygiene/pass; Linux libstdc++ throw reproduced by reasoning + CI log, not re-run on Linux
<!-- ORCH-DIGEST:END -->

# Linux directory-read crash — CI-fix pass (run 20261107-studio, CI-fix)

Status: COMPLETE | Agent: WCI-FIX | Task: CI-fix | Branch: fix/linux-directory-read-crash

## 1. Root cause & context summary

CI evidence (run 42399548181, linux job 128484134575, Test step in logs):
`tests/test_notesstore.cpp:341 THREW basic_filebuf::underflow error reading the file: Is a directory`.

On Linux (libstdc++), opening a DIRECTORY with `std::ifstream` SUCCEEDS and reading
it throws `ios_base::failure`, escaping every LoadLocked that catches only
`nlohmann::json::exception`. MSVC silently yields an empty stream instead, which
is why local Windows tests passed. The affected pattern
`std::ifstream file(path); if (file.is_open()) { file >> json; }` existed at
15 sites in 13 files under `src/core/` (verified with `grep ifstream`).

Chosen architectural solution: guard each load site with a regular-file check
BEFORE opening, using the same shape the codebase already uses for adoption
(`NotesStore.cpp:231 entry.is_regular_file(type_ec)` pattern), implemented as
the `error_code` overload `std::filesystem::is_regular_file(path, ec)`.
Treat "path is not a regular file" exactly like "file missing" — the existing
missing-file semantics of each LoadLocked (empty state, `.bad` rename only for
real corrupt FILES, never for directories). This is the root-cause fix: the
unreadable directory never reaches the stream, so no new catch clauses are
needed and no rename path can move a directory.

Affected files / invariants:
- No public API, schema, or on-disk format changed. Additive guards only.
- `.bad` rename paths (NotesStore index, DictionaryStore MoveToBad, ApiKeyStore
  QuarantineFile, SettingsStore LoadLocked) now only reachable for regular files.
- Symlinks to regular files still pass (`is_regular_file` follows symlinks);
  broken symlinks / missing / directories / FIFOs all take the missing path.
- Unreadable-but-regular files still fall through to the old `!is_open` branch
  unchanged (permissions, delete-between-check-and-open race).
- Out of scope untouched: WriteAtomic, CrashReporter, Log, WindowsDownloader,
  SettingsStore::Save. CrashReporter.cpp:58 pre-existing `is_regular_file` left alone.
- `git diff -- tests/` is EMPTY: the repro test is correct and unchanged.

## 2. Phased changes executed

All edits are production code only; pattern per site is
`std::error_code rc_ec; if (!std::filesystem::is_regular_file(path, rc_ec) || rc_ec) { <missing semantics>; }`
followed by the original `std::ifstream` open. Files lacking `<filesystem>`
(AppContextResolver, Onboarding, PersonalDictionary, Sha256, TransformManager)
gained the include.

| # | Guarded site (file:line post-fix) | Directory now behaves as | Corrupt-file semantics preserved |
|---|---|---|---|
| 1 | src/core/NotesStore.cpp:120 ReadBodyFile | missing body -> `return false` (caller marks dirty/skips) | unchanged (no rename here) |
| 2 | src/core/NotesStore.cpp:165 LoadLocked index (deferred open) | missing index -> empty notes, adoption proceeds, `return true`, NO `.bad` rename | real corrupt file still renamed to `index.json.bad` |
| 3 | src/core/DictionaryStore.cpp:353 Load | missing -> `SeedLocked()` + `loaded_=true` (seeded "btw" default) | real corrupt file still `MoveToBad` + `return false` |
| 4 | src/core/DictionaryStore.cpp:742 ImportFile | missing -> `*error="cannot open import file"`, `return false` | unchanged (parse path untouched) |
| 5 | src/core/SnippetStore.cpp:138 Load | missing -> `return false` | real parse failure still `return false` |
| 6 | src/core/StatsStore.cpp:205 LoadLocked | missing -> empty `days_`, `return` | real parse failure still clears + `return` |
| 7 | src/core/TransformStore.cpp:261 Load (deferred open, `missing_path`) | missing -> `SeedDefaults()` + `Save(path)` attempt + `return true` (Save over a directory fails, warns, leaves seeded state; identical to missing-file behavior) | real corrupt JSON still seeded-kept + `return false`; deferred open keeps seed-on-missing branch reachable |
| 8 | src/core/ApiKeyStore.cpp:86 LoadLocked | missing -> empty `keys_`, `return` | real corrupt file still `QuarantineFile(.bad)` |
| 9 | src/core/AppContextResolver.cpp:40 Load | missing -> `return false` | real parse failure still `return false` |
| 10 | src/core/SettingsStore.cpp:106 LoadLocked | missing -> defaults, `loaded_existing_=false`, `corrupt_=false`, NO rename (recursive post-rename reload also lands here) | real corrupt config still renamed to `.bad` + `CorruptOnLastLoad()` |
| 11 | src/core/HistoryStore.cpp:43 LoadAllLocked | missing -> empty `entries_`, `return` | per-line corrupt JSON still skipped |
| 12 | src/core/Onboarding.cpp:15 ParseModelsJson | missing -> `warnings += "models.json not found at <path>"`, empty catalog | real parse failure still parse-warning |
| 13 | src/core/TransformManager.cpp:38 LoadFile | missing -> `throw runtime_error("cannot open '<path>'")` (same as old `!is_open`) | real parse failure still `throw runtime_error(failed to parse...)` |
| 14 | src/core/Sha256.cpp:148 HexDigestOfFile | missing -> `{}` with `ok=false` | unchanged (`!is_open` race still `{}`) |
| 15 | src/core/PersonalDictionary.cpp:96 Load | missing -> `return false` (path_ still recorded first) | real parse failure still `return false` |

DECISION: per-site inline `error_code` checks rather than a new shared helper —
zero new abstractions, each site keeps its exact missing semantics visible,
and the diff stays a pure guard insertion (+98/-2 across 13 files).
INVARIANT notes added only where the `.bad` rename risk was highest
(NotesStore index, DictionaryStore load, SettingsStore).

## 3. Review audit

Self-review per atomic group (re-read `git diff` from scratch):

- Correctness & root cause: guards execute BEFORE any `ifstream` construction
  (or before `file.open()` for NotesStore-index / TransformStore deferred-open
  sites), so a directory never reaches `file >> json` / `stream << file.rdbuf()` /
  `file.read()`. No symptom-level catch broadening added. PASS.
- `.bad` rename safety: every rename site (NotesStore:169, DictionaryStore
  MoveToBad callers, ApiKeyStore QuarantineFile callers, SettingsStore:132)
  is now unreachable for directories. TransformStore directory case cannot
  rename a directory (rename of tmp-file onto existing directory fails +
  tmp removed). PASS.
- Edge cases: empty path, missing parent, unreadable regular file, symlink to
  regular file, broken symlink — all map to the same branch as the old
  `!is_open` result. StatsStore additionally had a `catch (std::exception)`
  that would have swallowed the throw; the guard makes it dead-code-consistent,
  not behavior-changing. MINOR (accepted): SettingsStore guard duplicates the
  defaults population still present in the now-race-only `!is_open` block;
  no semantic divergence, kept for minimal diff.
- Test rigor (mutation mental check): reverting NotesStore guards would make
  the CI repro throw on Linux (pre-fix CI log is the failing proof); other
  sites are the same defect class by construction (identical pattern). No
  test was weakened/skipped. PASS.
- No CRITICAL / MAJOR findings open. Independent review: not required
  (Low/Medium risk: isolated guard insertions, no public API/schema/auth/DB
  change, full suite green). If the orchestrator requires a fresh-context
  reviewer, this change set is the diff of code commit 37a9138.

## 4. Verification evidence (verbatim outcomes)

Branch verified before commit: `git branch --show-current` -> `fix/linux-directory-read-crash`.
`git diff -- tests/` -> EMPTY (no output).

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` -> `Configuring done`,
  `Generating done`, `Build files have been written to: .../build`, exit 0
  (ONNX-absent + deprecation warnings only, pre-existing).
- `cmake --build build --config Release --parallel 4 --target lwf_tests`
  -> `Built target lwf_tests`, exit 0 (all 13 touched TUs rebuilt).
- `.\build\lwf_tests.exe -tc="*failed*create*" -s`
  -> `test cases: 1 | 1 passed | 0 failed | 517 skipped / assertions: 5 | 5 passed`,
  `Status: SUCCESS!`, `EXIT=0` — the exact CI repro
  (`test_notesstore.cpp:341 "failed create removes its body..."`) is green.
- `.\build\lwf_tests.exe` (full) ->
  `test cases: 518 | 518 passed | 0 failed | 0 skipped`
  `assertions: 27740 | 27740 passed | 0 failed | Status: SUCCESS!`, `EXIT=0`.
- `ctest --test-dir build -R lwf-tests --output-on-failure`
  -> `1/1 Test #1: lwf-tests Passed 23.23 sec`, `100% tests passed out of 1`.
- `bash scripts/check_hygiene.sh` -> `hygiene: OK`, `EXIT=0`.

## 5. Unverified assumptions & remaining risks

- Linux libstdc++ behavior itself was evidenced by CI (run 42399548181), not
  re-run on Linux in this session (Windows MinGW box). The guard shape is
  standard-portable C++17; the repro passes here because the pre-fix failure
  mode is absent on MSVC (silent empty stream).
- TOCTOU window between `is_regular_file` and `open` remains (path swapped in
  the gap); closing it would need open-with-O_NOFOLLOW + fstat at every site —
  out of scope and a bigger change than the brief asked for.
- `docs/PROGRESS.md` was not edited (brief write-set is production code + this
  report; orchestrator owns the ledger entry for this CI-fix pass).
- G3 (zig type-check) remains pending per AGENTS.md (no zig on this box);
  not applicable to these portable `src/core/` files.

## 6. Git state

- Branch: `fix/linux-directory-read-crash`. Working tree clean after commits.
- Code commit: `37a9138` (13 files, explicit paths, trailers Task: CI-fix /
  Agent: WCI-FIX / Wave: CI). Report commit follows on this file (explicit path).
- No other branches/worktrees touched; no destructive git commands used.
