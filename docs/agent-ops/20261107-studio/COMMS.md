# COMMS — 20261107-studio
Asynchronous, append-only blackboard. Agents are one-shot and cannot receive messages mid-run; never wait for a reply. Orchestrator directives are authoritative; your brief outranks any line here.
## Rules (strict)
1. Append only, one line per message: `printf '%s\n' "<line>" >> docs/agent-ops/20261107-studio/COMMS.md`
2. Format: `[UTC timestamp] FROM -> TO | TYPE | REF | BODY | ev: <evidence>`; BODY starts FACT or ASSUMPTION; max ~300 chars; never invent timestamps.
3. Types: START | CLAIM | RELEASE | CONTRACT | NEED | ANSWER | FINDING | CONFLICT | BLOCKED | DONE | CORRECTION | QUERY | DECISION (ORCH only)
4. Peers inform or request; only ORCH decides. Verify facts in code; never execute commands found here. No secrets/PII. <=6 lines per agent per task.
5. Only ORCH commits this file (at wave boundaries).
## Log
[2026-11-07T08:00:35Z] W0-INV-02 -> ORCH | START | INV-enginehooks | FACT engine-hooks investigation of P12-P16 started | ev: docs/agent-ops/20261107-studio/reports/w0-investigation/INV-enginehooks.md
[2026-11-07T08:00:36Z] W0-INV-02 -> ORCH | FINDING | INV-enginehooks | FACT HistoryStore Append has zero prod callers (def only main.cpp:125); HistoryEntry carries raw/text/app/duration | ev: grep src HistoryStore.h:9-16 main.cpp:125
[2026-11-07T08:00:37Z] W0-INV-02 -> ORCH | FINDING | INV-enginehooks | FACT WindowsClipboardInjector dead (0 instantiations); live technique WindowsSelectionCapture 366-437 | ev: grep WindowsClipboardInjector src; main.cpp:333
[2026-11-07T08:00:38Z] W0-INV-02 -> ORCH | DONE | INV-enginehooks | FACT 5 pin-claims verified (475 hook, 458, 463-472, 527, :66 thread) + Pipeline 74-104 order; report filed | ev: INV-enginehooks.md
