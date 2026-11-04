# S5–S9 re-land — how the un-applyable bundle was resolved

## What happened

`feature-stt-providers-s5-s9.bundle` is an INCREMENTAL bundle: its header
names one prerequisite commit —

    -86624419bce49910708afe294600130f27d67c6d  Record decision B outcome and
                                                rebase report in phase 0 document

— and carries only the objects NEW to the five S5–S9 commits
(98b0829, 8928b0c, 3df580e, 16b6f51, df6b680 on `feature/stt-providers`).
The dev-environment reset wiped the repo that held 86624419, so `git fetch
<bundle>` / `git bundle unbundle` both abort with "Repository lacks these
prerequisite commits". The prerequisite is gone everywhere on disk; the
bundle can never be applied as-is in this environment.

## What was recovered from the bundle anyway

The bundle's pack section was split off and unpacked (`scripts/split_bundle.py`,
`scripts/dump_pack.py`). Salvaged whole: both `src/stt/cloud/` trees, the
final `docs/` tree, the bench tree, all five commit objects, and 37 blobs —
including the verbatim final text of GnaniProvider.cpp, every ElevenLabs
file, HttpStatus, LanguageMap, MuseContract/Provider/Request/Response.h, all
six S5–S9 test files, the final docs/STT_CLOUD.md and docs/BENCHMARKS.md,
plus the intermediate ConsentGate.cpp / consent_test.cpp versions that proved
the R1 snapshot already carried 0001's standalone files.

Blobs that deltified against the lost prerequisite are gone for good:
SttRouter.{cpp,h}, ISttProvider.h, GnaniProvider.h, the Gnani request/
response files, ClipSplitter, WavEncoder, MultipartBuilder, main.cpp,
CMakeLists.txt, AGENTS.md, README.md, check_hygiene.sh, the final
PROGRESS/QUESTIONS/MANUAL_TEST_WINDOWS.

## How the repo was rebuilt (/home/z/my-project/Local-Wisper-Flow)

    80cf844  Restore R1 working tree: v0.1.0-rc1 line + R0 audit + R1 GUI redesign
    befc8f8  Add consent gate, secret redaction, and cloud consent settings UI   (S5)
    4d1ea3a  Add cloud STT latency and cost benchmark                            (S6)
    b4d23ef  Add ElevenLabs Scribe cloud provider with language mapping          (S7)
    dcc5626  Add Meta Muse Voice Transcribe cloud provider                       (S8)
    fd03736  Complete the S0-S9 documentation pass                               (S9)

- Base: `download/Local-Wisper-Flow-R1.zip` (the R1 GUI-redesign line,
  which already contained S5-0001's standalone ConsentGate/Redact files).
- Every recovered blob landed byte-identical; lost S0–S4 support files were
  reconstructed against the recovered providers, tests and docs, and the
  cloud settings UI was re-homed from the old SettingsWindow tab to the R1
  Full window's Cloud page (per the R1 redesign, which had deleted
  SettingsWindow).
- Gates on the final tree (Linux core, gcc 14.2, Release): build -j4 clean,
  214/214 doctest cases / 9959 assertions green, check_hygiene.sh OK +
  --self-test OK, check_no_network.sh OK, lwf-bench and lwf-cloud-bench
  smokes run, app smoke exits 0 printing stt_provider/stt_consent.

## This directory

The five `0001-…`–`0005-…` patches are `git format-patch` exports of the
re-landed S5–S9 commits. Apply onto the base (or any tree that already has
the R1 redesign + the S5 support layer) with `git am 000*.patch`.

Still owed (docs/QUESTIONS.md in the repo): the live-API smoke for
Gnani/ElevenLabs/Muse, the Windows build/UI checkpoint, and the zig
type-check of the Windows-only sources (G3).
