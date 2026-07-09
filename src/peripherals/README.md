# peripherals — slot-side input/output device adapters

Folder responsibilities per [`../CLAUDE.md`](../CLAUDE.md) (src/ conventions). Every device
here is deterministic and machine-wired in `src/machine/hbf1xv_machine.cpp`.

## File map

| File | Role |
|---|---|
| `keyboard_matrix.{h,cpp}` | 11×8 MSX keyboard matrix; row selected via PPI port C, read via port B. |
| `msx_key_names.{h,cpp}` | Canonical 71-entry key-name table (M27) shared by input scripts and the SDL3 mapper; a hard cross-consistency test keeps both tables identical. |
| `joystick.{h,cpp}` | Joystick ports 1/2 through PSG port A/B (stick directions + triggers). |
| `rensha_turbo.{h,cpp}` | Rensha (auto-fire) turbo dial behavior with its own clock source. |
| `cassette_interface.{h,cpp}` | Cassette motor/read/write line surface (fidelity depth = backlog F1, unscheduled). |
| `printer_port.{h,cpp}` | Centronics printer port surface (dot-matrix rendering depth = backlog F2, unscheduled). |
