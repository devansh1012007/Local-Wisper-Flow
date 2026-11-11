# QUESTIONS

Open questions for Devansh. Each entry names the task, the question, and the
evidence gathered so far.

1. G3 zig type-check: this Linux box has no zig binary and no network access to
   the zig release mirror used previously. Every task touching `src/os/*.cpp`,
   `src/ui/*.cpp` or the `_WIN32` branch of `main.cpp` records G3 as pending.
   Options: install zig here, or run `zig c++ -fsyntax-only -target
   x86_64-windows-gnu -Isrc <file>` on a machine that has it.
   Redesign note (2026-11-08, `ops/ui-full-redesign` Ph2-ShimDel + Ph7 docs):
   G3 remains pending here — no zig on the dev box; Ph7 changes are docs
   only (no build needed). T13a (2026-11-09, `ops/ui-redesign-v2`):
   G3 again pending — no zig; touches `src/ui/MainWindow.{h,cpp}` +
   `src/core/Onboarding.{h,cpp}` with `os/WindowsDownloader.h` from the
   UI TU (`zig c++ -fsyntax-only -target x86_64-windows-gnu -Isrc
   src/ui/MainWindow.cpp` owed).
   T16/T17 (2026-11-10, `ops/ui-redesign-v2`): docs-only sweep + external
   skill rewrite — no Windows sources touched, so nothing new for zig; the
   T13a owed syntax-only run above still stands.

2. D8 model hashes: model URLs and SHA-256 values in `models.json` must be copied
   from the real model pages, never invented. Unverified entries ship with
   `"sha256": ""` and the UI shows an amber "unverified file" warning.
   Needed from Devansh: confirmation of the exact ggml whisper model and Qwen2.5
   1.5B Instruct Q4_K_M gguf URLs he wants bundled in the onboarding list.

3. B2 Start-menu suppression: the unassigned-VK trick between Win down/up needs a
   real Windows keyboard to confirm. Manual checklist item, cannot be verified here.

4. F2 history purge: the dependency tarballs were removed from the index
   (git rm --cached) but their blobs remain in git history, so a fresh clone
   still downloads ~179 MB of old objects. Rewriting history needs
   `git filter-repo` and a force-push and must be a manual decision:
   wanted or not? Until then, clones pay the size; the working tree and CI
   do not need the tarballs (scripts/fetch_deps.sh|.ps1 rebuild them).

5. S0/S4/S7/S8 live verification: the sandbox cannot reach api.vachana.ai,
   api.elevenlabs.io or api.meta.ai, so every cloud provider is unit-tested
   with fakes only. Needed from Devansh on a Windows machine with real keys:
   run the Windows build + ctest, then `python scripts/gnani_smoke.py --file
   tests/data/sample.wav` with GNANI_API_KEY set (adjust GnaniContract.h if
   the live shape differs), then a dictation per provider (gnani, elevenlabs,
   muse) following the S4/S5-S8 checklist in docs/MANUAL_TEST_WINDOWS.md.
   Until then cloud mode should be treated as contract-verified, not
   integration-verified.

6. Provider pricing for the cost model: lwf-cloud-bench models
   `cost = audio_hours x price_per_hour` and ships with the one verified
   constant (Muse Voice Transcribe $0.18/h, vendor page 2026-11-04). Gnani and
   ElevenLabs prices are not modeled; if Devansh wants all three compared in
   docs/BENCHMARKS.md, supply the current per-hour prices from the vendor
   pages.

7. Streaming double-commit race: FIXED 2026-11-06/07 on `fix/gui-typography-dpi`.
   `DictationEngine::MaybeCommitStreamingSegment` now holds `commit_mutex_` for
   the whole check-slice-advance-commit sequence, so two concurrent `Tick`
   callers cannot commit the same segment twice (commit 8223834). Verified by a
   deterministic 8-thread barrier regression test (fail-before verified: 4/4 runs
   produced 8 duplicate commits; pass-after 5/5). The `Tick` header comment was
   narrowed to the actual guarantee (serialized commit only; concurrent
   `StopAndTranscribe` double-run still possible and now documented, b0cfe38).
   Remaining follow-up: a Tick-serialization task if the engine ever calls Tick
   from more than one thread.
