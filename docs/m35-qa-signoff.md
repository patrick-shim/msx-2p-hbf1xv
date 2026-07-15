# M35 QA Sign-off — Multi-disk hot-swap

**Verdict: PASS** (coordinator-authored; the delegated `msx-qa` agent's run was invalidated — see
"QA-process incident" below). Tag: **v1.0.36**.

## Headline: live human validation

The human ran the real build end-to-end: booted YS II from `disk1.dsk`, reached the genuine
"INSERT DATADISK IN DRIVE A - RET" prompt, pressed **F11** (window title flipped to `disk2.dsk`,
stderr logged the swap), pressed RET, and **the game read disk 2 and continued into gameplay**.
This is the milestone's authoritative acceptance signal (screenshots on record). The feature works
on real software.

## Automated evidence (independently reproduced by the coordinator)

- Build: headless **183/183**; SDL3-ON **194/194** via `tools/bootstrap-build.ps1` (the two new
  M35 executables — `frontend_sdl3_input_mapper_disk_swap_unit_test` #189 and
  `frontend_sdl3_app_multi_disk_integration_test` #195 — both green). Reproduced multiple times
  from a clean bootstrap.
- Hard constraints: ZERO `src/devices/cpu/` or `src/core/` edits (frontend-only; the swap CALLS
  the existing `disk_drive().set_disk_changed(true)` / `attach_image()` public APIs — no
  `src/devices/fdc/` logic edits). ZEXALL correctly withheld per DEC-0048.
- Media-change design: re-attach a fresh `DiskImage` + `set_disk_changed(true)` is faithful to the
  DSKCHG line read at `0x7FFD` bit2 (`src/devices/fdc/sony_fdc.cpp:42-44`,
  `disk_drive.h:82-83`) — corroborated by the game actually reading disk 2 on RET.

## Adversarial mutation results (coordinator-run, after the QA agent failed)

| Mutation | Target test | Result |
| --- | --- | --- |
| A — disable the F11 branch in `sdl3_input_mapper.cpp` | `disk_swap` unit | **FAILED as required** (3 cases) — the unit test genuinely discriminates F11 handling. |
| B — disable the disk-index rotation in `on_disk_swap_hotkey()` | `multi_disk` integration | **STILL PASSED** — the integration test did NOT catch a broken swap. See residual R-M35-1. |
| C — disable the parser `push_back` accumulation | `sdl3_cli` unit | Not completed (sweep stopped early to un-block the human's live test). |

## Residuals / findings

- **R-M35-1 (test quality, non-blocking):** the `multi_disk` integration test passes even when the
  disk-index rotation is disabled — it validates the config/parse + attach path but does not assert
  that the drive-A disk actually ADVANCES to disk N on F11 (the SDL3 dummy-driver environment can't
  drive F11 through a real event loop, so the swap action isn't exercised end-to-end in the test).
  The live human playtest is the authoritative proof for now. Strengthen this test in a later cycle
  (assert the inserted-disk index/name changes after a swap). Does NOT gate closure — the feature is
  live-proven.
- **QA-process incident (governance, recorded in DEC-0049):** the delegated `msx-qa` agent's
  adversarial-mutation step used `git checkout` to undo mutations on UNCOMMITTED work, which
  reverted two files (`sdl3_cli.cpp`, `sdl3_input_mapper.cpp`) to pre-M35 and produced a FALSE
  "critical compile defect" FAIL. The coordinator diagnosed it (the developer's code had built
  194/194 before and after restore), restored both files from intact sources, committed a
  protective checkpoint (`e360a5b`) so the tree is clean, and completed the adversarial validation
  directly. Future QA adversarial mutation MUST be non-destructive (edit-then-revert-the-edit, or
  operate only on committed state) — see the updated `msx-qa` agent rule.

## Disposition

The feature is implemented frontend-only, live-validated by the human, and passes the automated
suite (194/194) with mutation A confirming real test coverage of the core F11 path. R-M35-1 (an
integration-test strengthening) is carried forward. **PASS → tag v1.0.36.**
