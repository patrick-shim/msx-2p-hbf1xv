

## M11-S6 context: S1985 behaviours validated by this diff

The probe tests/parity/m11_bus_probe.bin (26 bytes, base 0xC000, SHA-256
f8be4803533d77147e5ddf398415046f093168dd7bb8796342a51aec8b799455) exercises the
residual S1985 MSX-ENGINE I/O whose result lands in a CPU register, so the shared
M10 register-diff comparator captures it. Every one matched the genuine Sony
HB-F1XV machine in openMSX 19.1 (its machine XML instantiates a real <S1985> with
the 5-bit mapper read-back, confirmed in
/usr/share/openmsx/machines/Sony_HB-F1XV.xml):

| S1985 behaviour (fact-sheet ref) | probe op | expected | A (emu) | B (HB-F1XV) |
|---|---|---|---|---|
| mapper read-back 100xxxxx (sec.4) | OUT (#FC),#25 ; IN A,(#FC) | #85 = 0x80 OR (0x25 AND 0x1F) | #85 | #85 |
| switched-I/O device ID 0xFE, ~ID (sec.6) | OUT (#40),#FE ; IN A,(#40) | #01 = ~0xFE | #01 | #01 |
| 16-byte backup-RAM round-trip (sec.6) | addr=5 ; write #3C ; IN A,(#42) | #3C | #3C | #3C |

Result registers B=#85 (mapper), C=#01 (~ID), D=#3C (backup RAM) are identical on
both, and every intermediate architectural field (PC, opcodes, R, flags) matches.

## Scope / honesty notes

- The probe is deliberately slot-safe and BIOS-independent: it only writes the
  page-0 mapper segment (never the page-3 code it runs in) and the switched I/O,
  so the identical architectural sequence is reproducible on the real machine
  regardless of BIOS/RAM slot layout. A free-running slot-manipulating trace is
  intentionally NOT used for A/B because the M11 bring-up slot default (#A8=0xFF,
  risk R-1) differs from a real BIOS boot (#A8=0, slot-0 BIOS); that authentic
  reset is M12 scope once ROMs populate slot 0.
- The #98-#9B -> #9C-#9F and #A8-#AB -> #AC-#AF straight-alias mirrors are verified
  by unit tests (devices_chipset_io_bus_unit_test) and the machine integration test
  (machine_hbf1xv_system_bus_integration_test, #AC==#A8); they are not in this probe
  because reading #A8/#AC yields BIOS-dependent absolute values that would not be
  state-identical across the two initial conditions (only the mirror invariant is
  machine-independent).
- Reproduce with: tools/openmsx-io-parity.ps1 (drives openMSX HB-F1XV on WSL and
  regenerates this artifact). This is a REAL captured diff (trace-diff.py exit 0),
  not a fabricated result.
