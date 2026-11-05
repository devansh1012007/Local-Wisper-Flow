# QUESTIONS

Open questions for Devansh. Each entry names the task, the question, and the
evidence gathered so far.

1. G3 zig type-check: this Linux box has no zig binary and no network access to
   the zig release mirror used previously. Every task touching `src/os/*.cpp`,
   `src/ui/*.cpp` or the `_WIN32` branch of `main.cpp` records G3 as pending.
   Options: install zig here, or run `zig c++ -fsyntax-only -target
   x86_64-windows-gnu -Isrc <file>` on a machine that has it.

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

7. Streaming double-commit race: with the current fakes, two concurrent
   `DictationEngine::Tick` callers can both pass the pause/commit
   check-then-act in `MaybeCommitStreamingSegment` and commit the same
   segment twice. Production calls `Tick` only from the engine's `Run()`
   worker thread, so this is latent; the streaming tests now drive
   `ProcessOne` synchronously and pass deterministically. Worth a real
   atomic-claim fix in src/core before any engine-thread changes.
