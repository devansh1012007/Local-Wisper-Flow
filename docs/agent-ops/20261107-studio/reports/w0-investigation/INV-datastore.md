# INV-datastore — P02–P11 data layer verification

## ORCH-DIGEST
All P02–P11 plan claims verified except two corrections: (1) HistoryStore atomic pattern has NO `.bak` step; (2) `text_mutex_` is DictationEngine's, not a store pattern. No investigation template exists in templates/ [VERIFIED glob]; using Standing Orders tagging. Branch `feature/studio-pages` @ `0e655ec` [RAN git branch/log]. All [VERIFIED] below are direct reads.

## P02 AtomicFile — GAP confirmed
- [VERIFIED glob src/core/AtomicFile* → No files found]: helper absent, must be created.
- [VERIFIED src/core/SettingsStore.cpp:217-228]: `tmp=path+".tmp"`, ofstream trunc, `<<dump(4)<<"\n"`, flush, `copy_file(path→path+".bak")`, `rename(tmp→path)`. Exact extent: lines 217–228 only.
- [VERIFIED src/core/HistoryStore.cpp:81-108]: same tmp+flush+rename in `CompactLocked`, but NO `.bak` copy. Plan's "duplicated pattern" is approximate — P02 factoring SettingsStore shape (with .bak) does not match HistoryStore. Confidence: high.

## P06/P07 load-only — confirmed
- [VERIFIED grep Save in src/core → only ConfigManager.cpp:11, SettingsStore.cpp:190]: no Save in SnippetStore/PersonalDictionary.
- [VERIFIED src/core/SnippetStore.h:13-23]: `Load/SetEntries/SetMode/Entries/Warnings/Loaded/Expand/Normalize` only. Load at SnippetStore.cpp:76-128 parses `[{trigger,expansion}]`.
- [VERIFIED src/core/PersonalDictionary.h:9-20]: `Load/SetWords/SetReplacements/AddWord/Words/Replacements/Loaded/InitialPrompt/Enforce` only. Load at PersonalDictionary.cpp:89-139 accepts array-of-strings OR object `{words,replacements:[{from,to}]}`. Neither class has a mutex → not thread-safe today.

## P08/P10 TransformManager — confirmed
- [VERIFIED src/core/TransformManager.cpp:38]: `throw runtime_error` on missing file. :63-65 throws if prompt lacks `{text}`.
- [VERIFIED transforms.json:1-14]: v1 array `[{name,prompt}]` x3 (Fix Grammar/Make Formal/Summarize), all contain `{text}`. Loader expects exactly this shape [VERIFIED TransformManager.cpp:49-58].
- [VERIFIED installer/lwf.iss:43]: `Source: "..\transforms.json"` seeded to install dir. So fresh install = v1 present, not missing.

## P11 app_profiles — shape verified, `category` absent, writer unknown
- [VERIFIED src/core/AppContextResolver.h:9-16]: `AppProfile{name,prompt_addendum,casual,trailing_period,use_llm,paste_chord}` — no `category` field.
- [VERIFIED src/core/AppContextResolver.cpp:45-66,69-115]: file shape `{"profiles":{<key>:{prompt_addendum,casual,trailing_period,use_llm,paste_chord}},"rules":[{"match":{process,title_contains,title_regex},"profile":<key>}]}`; rules ignore unknown fields, so additive `category` is safe to add.
- [VERIFIED src/main.cpp:335-336]: loads `data_dir+"\app_profiles.json"`; fallback `UseBuiltInProfiles()` (:126-170) hardcodes email/chat/code/notes/default. [UNKNOWN] who writes the shipped `app_profiles.json` — no Save anywhere in repo (grep 0 hits); treat as hand-authored/untracked. P11 must add Save + seed defaults.

## Gate allowlist (P02–P09 + A1)
- [VERIFIED scripts/check_no_network.sh:30]: allowlist is `(CrashReporter|HistoryStore|Log|SettingsStore)\.cpp` — .cpp only, despite scanning .h too.
- Needed additions (same-commit): `AtomicFile`, `StatsStore`, `DictionaryStore`, `SnippetStore`, `TransformStore`, `NotesStore`, `ApiKeyStore` (.cpp each). [ASSUMED] headers stay write-API-free (use helper only in .cpp) or allowlist must include .h too.

## HistoryStore record + locking (for P05 StatsStore)
- [VERIFIED src/core/HistoryStore.h:9-16]: `{timestamp,raw,text,app_profile,duration_ms,language}`; JSON keys `ts/raw/text/app/dur/lang` [VERIFIED HistoryStore.cpp:54-59,89-102]; omits empty optional fields on write.
- [VERIFIED src/core/HistoryStore.h:38 + HistoryStore.cpp:19,25,112]: `mutable std::mutex mutex_` + `lock_guard` on every public op; `CompactLocked` is called with lock held. Same in SettingsStore [VERIFIED SettingsStore.h:58].
- [VERIFIED src/core/DictationEngine.h:116]: `text_mutex_` belongs to DictationEngine, NOT a store pattern — plan wording "text_mutex_ pattern" is misleading; copy HistoryStore's `mutex_` style instead.

## Untouched remainder
P03/P04/P05/P09 are new files (no source to gap-check). Long-run: Source clean; Load must handle 4 legacy shapes + .bak; Hostile = corrupt JSON (Load returns false today) must not clobber; Operability = G4 same-commit allowlist is the brittle point; Reversal via git revert + .bak restore.
