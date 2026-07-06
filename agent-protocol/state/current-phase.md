# Current Phase

- Objective: M15 (Device Integration + S1985 Chipset Full Wiring) CLOSED (DEC-0010), tagged v1.0.15 — five milestones M11-M15 now tagged (v1.0.11..v1.0.15). Now driving M16 = FDC (Fujitsu MB89311 + 720 KB 3.5" drive) per the human "orchestration until M16" directive. Order: M11..M15 done -> M16 (FDC, active).
- Active Phase: M16 Planning (planner-first)
- Phase Owner: MSX Master Agent (coordinator)
- Phase Status: M15 closed + tagged v1.0.15 (residual planning-only drift cleaned per orchestration finding). Backlog B1/B2/C4/C6 -> DONE (M15); C5 IN-PROGRESS. M16 opened (REQ-M16-001): FDC wired onto the slot-3-2 DISK ROM (M13); backlog B8 -> IN-PROGRESS (M16). Planner package requested (REQ-M16-002). Per the human directive, drive planner -> developer -> QA and STOP at M16 QA sign-off for the human release decision (M16 keeps the normal gate; no auto-close).
- Entry Criteria (M16): M15 Done (met); planner package required before developer implementation.
- Exit Criteria (M16): MB89311 FDC + 720KB drive implemented + wired into the M11 io_bus / slot-3-2 DISK ROM path; deterministic disk-image read/seek/status; boot advances past the M15 checkpoint through the Disk-ROM handshake; deterministic unit + system-integration tests; real openMSX A/B trace-diff; zero regression M1-M15; QA sign-off; human release decision.
- Open Blockers: None. Watch-items: disk-image handling must be deterministic (no host-disk nondeterminism); no A/B parity claim without a genuine captured trace-diff; keep FDC device logic in src/devices/ and machine wiring in src/machine/ per src/CLAUDE.md.
- Updated At: 2026-07-06T18:00:00+09:00
