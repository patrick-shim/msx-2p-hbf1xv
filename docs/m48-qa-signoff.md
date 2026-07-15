# M48 QA Sign-off — V9958 command-engine contention model (ban-respecting)

**Recommendation: PASS** (2026-07-14). Release target **v1.1.5**.

Scope: DEC-0075 / backlog D4. Slice 1 (per-line throughput cap `slot_factor = 154 / S_effective`,
factors 1.00/1.75/4.97) + Slice 2 (CPU-concurrent `#98` slot-steal, one-directional). Timing/STATUS
overlay only — atomic VRAM, `vdp_command_engine.*` + M44 render-sync + `src/core`/`src/devices/cpu`
untouched.

## Gate results (real output)
- **EG-1/EG-2 assets:** validate-assets PASS; checksums → docs/asset-checksums.txt.
- **EG-3 full ctest:** 231/231, 0 failed (incl. the 2 new M48 non-tautological oracles).
- **EG-5 ZEXALL/ZEXDOC full sweep (MANDATORY — `#98` CPU↔VRAM path touched):** **PASS**,
  1662.07 s, 0 failed, EXITCODE=0. Confirms CPU functional correctness intact.
- **EG-6 13-mode screen A/B:** **PASS** — structural + oracle-corroborated (no rendering input
  changed → modes bit-exact by construction). Automated openMSX re-run hit a harness error in this
  environment; documented, non-blocking. (debug/m48/screen-ab/m41-ab-report.md)
- **EG-7 command-timing A/B:** **PASS** — rigorous static source A/B of both models vs tier-1
  fact-sheet §8; dynamic CE-duration is not measurable by the available (opcode-trace) harness.
  (docs/m48-openmsx-ab.md)
- **EG-8 non-tautology:** proven — oracles re-derive from the tier-1 constants; adversarial factor/
  count/BL/SPD mutations fail.
- **EG-9 ban attestation:** every `VDPAccessSlots` hit in `src/` is a ban-documenting comment; no
  per-slot POSITION value transcribed/re-derived; numeric surface = 154/88/31/1368/6 only.
- **AC-4 (CPU byte-identical):** `git diff -- src/core src/devices/cpu` EMPTY; per-instruction
  lockstep witness (busy-vs-idle) byte-identical; M23 gating fixtures unchanged.

## Residual risk
- **R3 (4.97× sprites-on aggressiveness):** DISCHARGED — owner live spot-check of Laydock 2 + YS II:
  looks right, not too slow. Tier-1 wins over the DEC-0069 eye-tuning per DEC-0073.
- **R6 (ban ceiling):** accepted — this is the aggregate-throughput model; the cycle-exact per-slot
  gap structure stays UNBUILDABLE-WITHOUT-FABRICATION (the banned openMSX position table) and remains
  the open C1/D4 remainder. "Parity" here = §8 counts on average (direction + rough magnitude), with
  pixel output and CPU timing bit-exact.

Sign-off: PASS → owner release decision (v1.1.5).
