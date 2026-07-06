# Current Phase

- Objective: M13 (RAM/ROM Memory Devices & Slot Population) CLOSED by human release decision (DEC-0006 / REQ-M13-005); tagged git `v1.0.13`. Now driving M14 (Yamaha V9958 VDP incl. 128 KB VRAM). Order: M11(v1.0.11) -> M12(v1.0.12) -> M13(v1.0.13) -> M14(VDP, active).
- Active Phase: M14 Planning (planner-first)
- Phase Owner: MSX Master Agent (coordinator)
- Phase Status: M13 closed + tagged v1.0.13; deferred-scope backlog established (agent-protocol/state/deferred-backlog.md, DEC-0005) — every future planner must consult it. M14 kicked off (REQ-M14-001) + planner package requested (REQ-M14-002). M14 owns deferred-backlog item B9 (VRAM/VDP). Normal human-release-decision gate (no auto-close grant unless separately granted). No production code yet for M14.
- Entry Criteria (M14): M13 Done (met); planner package required before developer implementation.
- Exit Criteria (M14): V9958 device owns 128 KB VRAM (vram_ migrated out of the machine); register/status/port/VRAM/palette/indirect + VBlank/line interrupt behavior implemented + unit-verified; wired onto the M11 io_bus (S1985 #9C-9F mirror) + coordinated with the M12 CPU IM1 seam; deterministic unit + integration tests green; evidence gates green; real openMSX A/B trace-diff (VRAM now comparable); QA sign-off; human release decision. Rendering/timing DEPTH boundary to be set by the planner with deferred items recorded in the backlog.
- Open Blockers: None. Watch-items: V9958 is the most complex chip — must be sliced; no A/B parity claim without a genuine captured trace-diff; planner must update deferred-backlog.md for any rendering/timing depth pushed to a later milestone.
- Updated At: 2026-07-06T13:00:00+09:00
