# Docs index

What lives under `docs/`, and what to trust. Swept 2026-11-10 (T16,
`ops/ui-redesign-v2`).

## Current — keep accurate when the code changes

| File | What it is |
|---|---|
| `design/DESIGN.md` | **Hairline design specification — the source of truth for the UI identity** (Graphite/Paper palettes, type scale, metrics, contrast policy). Signed off 2026-11-09. |
| `ARCHITECTURE.md` | Module map and the fixed pipeline (core / os / ui / stt / llm / audio / vad). |
| `STT_CLOUD.md` | Cloud STT stack S0-S8: consent gate, providers, vendor contracts, the S5 in-app surface. |
| `MANUAL_TEST_WINDOWS.md` | Real-hardware manual checklists (A / C / D / E / F / S / R / R2 / MIC / OR / **V2** sections). Section V2 consolidates the ui-redesign-v2 checks and owed items. |
| `UI_AUDIT.md` | The D0 audit baseline, defect register D1-D14, and the dated status updates (§5.1 R1, §7 Ph2, §8 Hairline). |
| `BENCHMARKS.md` | `lwf-bench` / `lwf-cloud-bench` numbers and how to read them. |
| `QUESTIONS.md` | Open questions for Devansh, including the G3 (zig) ledger. |
| `PROGRESS.md` | Append-only task ledger — one row per task with gates run. New work records itself here. |

## Historical — do not edit, do not use for new work

| Path | What it is |
|---|---|
| `design/mockup.html` | Pre-Hairline mockup ("Ivory and Brass" phase). Superseded by `DESIGN.md`; carries a legacy banner under the title. |
| `STUDIO_PLAN.md` | The 2026-11-07 studio-pages plan (F0). Visual system superseded by Hairline; banner at the top. |
| `STUDIO_CHECKLIST.md` | The F9 sweep checklist for the studio visual system. Superseded; banner at the top. |
| `plans/` | Dated plan documents. The three `2026-11-08-full-ui-redesign*` files carry SUPERSEDED banners (the v2 plan lives outside the repo: `C:\Users\devan\.opencode\plan\ui-redesign-v2-draft.md`); the remaining files are completed feature plans kept for reference. |
| `agent-ops/` | Point-in-time multi-agent operation records (briefs, reports, gates, screenshots, capture scripts). Never rewritten. |

## Absent on purpose (as of 2026-11-10)

This repo has no `adr/`, `runbooks/`, `operations.md`, `changelog.md` or
`tech-debt.md`. Design decisions are recorded in `DESIGN.md` (incl. §12 open
decisions), agent working rules and ops knowledge in `AGENTS.md`, per-task
history in `PROGRESS.md`, and the gate ledger in `QUESTIONS.md`. Create such
directories/files only if a future task explicitly calls for them.
