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
