# Third-party notices

Echo flow (this repository) is MIT licensed (see `LICENSE`). It builds on the
third-party components below. Each entry names the version pinned in the
build, its license and where the pinned source comes from. Licence texts for
MIT, Apache-2.0, BSD-3-Clause and the zlib license are included by reference
to the upstream repositories; none of these components are modified in
behaviour except where a patch file in `patches/` is applied at build time
(the whisper.cpp ggml alias guard - a build-system-only change).

Build-system dependencies (compiled into the shipped exe):

| Component | Pinned version | License | Source |
|---|---|---|---|
| whisper.cpp | v1.9.3 | MIT | https://github.com/ggml-org/whisper.cpp (FetchContent) |
| ggml (bundled in whisper.cpp / llama.cpp builds) | vendored | MIT | https://github.com/ggml-org/ggml |
| llama.cpp | b10603 | MIT | https://github.com/ggml-org/llama.cpp (pinned tarball, `scripts/fetch_deps.sh`) |
| miniaudio | 0.11.25 | MIT-0 / Public Domain (dual) | https://github.com/mackron/miniaudio |
| gRPC | v1.71.0 | Apache-2.0 | https://github.com/grpc/grpc (pinned full tarball with in-tree third_party) |
| abseil-cpp | pinned in gRPC v1.71.0 | Apache-2.0 | https://github.com/abseil/abseil-cpp |
| protobuf | pinned in gRPC v1.71.0 | BSD-3-Clause | https://github.com/protocolbuffers/protobuf |
| re2 | pinned in gRPC v1.71.0 | BSD-3-Clause | https://github.com/google/re2 |
| c-ares | pinned in gRPC v1.71.0 | MIT-style (c-ares license) | https://github.com/c-ares/c-ares |
| zlib | pinned in gRPC v1.71.0 | zlib | https://github.com/madler/zlib |
| BoringSSL | pinned in gRPC v1.71.0 | OpenSSL-style + Apache-2.0 (mixed, see upstream) | https://boringssl.googlesource.com/boringssl |
| Dear ImGui | v1.92.9b | MIT | https://github.com/ocornut/imgui |
| GLFW | 3.4 | zlib | https://github.com/glfw/glfw |
| nlohmann/json | v3.12.0 | MIT | https://github.com/nlohmann/json |
| ONNX Runtime | prebuilt (drop-in `third_party/onnxruntime`) | MIT | https://github.com/microsoft/onnxruntime |

Build/test-only dependencies (not shipped):

| Component | Pinned version | License | Source |
|---|---|---|---|
| doctest | v2.4.12 | MIT | https://github.com/doctest/doctest |

Runtime data files (downloaded on first run, with an explicit click):

| Artifact | License | Source | Checked |
|---|---|---|---|
| Whisper `ggml-*.bin` weights (e.g. ggml-base.en.bin, ggml-small.bin, ggml-large-v3-turbo.bin) | MIT | https://huggingface.co/ggerganov/whisper.cpp (official whisper.cpp release weights) | 2026-11-13 |
| Silero VAD model (`silero_vad.onnx`) | MIT | https://github.com/snakers4/silero-vad | confirm the exact file's header before redistributing it inside the full/offline installer |
| Qwen2.5-1.5B-Instruct `q4_k_m` GGUF | Apache-2.0 | https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF (official Qwen GGUF repo; base model card https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct) | 2026-11-13 |

Cloud speech-to-text providers (strictly opt-in behind the consent gate in
Settings > Cloud; nothing is sent until the user enables a provider and
stores an API key). Their terms are the user's contract with the vendor, not
with this project:

| Provider | Endpoint host | Terms |
|---|---|---|
| Gnani.ai | api.vachana.ai | https://gnani.ai/terms-of-use/ (verify before release) |
| ElevenLabs | api.elevenlabs.io | https://elevenlabs.io/terms (verify before release) |
| Muse (Meta) | meta.ai endpoints per `src/stt/cloud/` contracts | vendor terms as documented in `docs/STT_CLOUD.md` |
| OpenRouter (optional LLM cleanup path) | openrouter.ai | https://openrouter.ai/terms (verify before release) |

Fonts: the GUI currently renders with the Dear ImGui default bitmap font.
If a TTF font is added later it must be licensed under the SIL Open Font
License (OFL) or equivalent redistributable license, and its copyright
notice must be appended here.

If you redistribute Echo flow, keep this notice, the LICENSE file, and the
license of every component you ship with it. The full/offline installer
variant additionally bundles the model weights listed above - their licenses
(MIT, Apache-2.0) permit redistribution with attribution preserved.
