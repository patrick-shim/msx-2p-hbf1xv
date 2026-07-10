# openMSX V9958 VDP-IRQ Register-Write /INT Parity (Sony HB-F1XV)

Reproducible A/B evidence for the M36 Bug B fix (V9958 IE0/IE1 register-write
/INT re-evaluation). Regenerate with `tools/openmsx-vdp-irq-parity.ps1`.

- Captured (UTC): 2026-07-10T07:54:46Z
- Reference: openMSX on WSL (openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC), machine `Sony_HB-F1XV` (genuine V9958).
- Grounding: openMSX `references/openmsx-21.0/src/video/VDP.cc:1186-1198` (`changeRegister` case 1 IE0 re-eval); debug write routes through the same `changeRegister` (VDP.cc:1615-1623); status read is the side-effect-free `peekStatusReg` (VDP.cc:1635-1638).
- Our fix: `src/devices/video/v9958_vdp.cpp` `change_register` case 1.
- Unit test: `tests/unit/devices/video_v9958_ie0_register_write_irq_unit_test.cpp` (`devices_v9958_ie0_register_write_irq_unit_test`).

## Probe

Park the Z80 in a `JR $` spin at 0xC000 with `iff=0` (nothing reads S#0), clear
R#1 IE0 and run a few frames so a VBlank latches the S#0 F flag WITHOUT asserting
/INT, then toggle R#1 IE0 and read the `VDP.IRQvertical` probe after each edge.

## Result: PASS (0->1->0->1)

```
OMIRQ S0=9F IV_baseline=0 IV_ie0set_Fpending=1 IV_ie0clear=0 IV_ie0set2=1
```

| Reading | openMSX | Meaning | Grounds |
| --- | --- | --- | --- |
| `IV_baseline` | 0 | IE0 off, F pending -> /INT NOT asserted | latch F without IE0 (VDP.cc:404 gate) |
| `IV_ie0set_Fpending` | 1 | IE0 set with F pending -> re-assert | VDP.cc:1189-1194 (Andonis/Zanac) |
| `IV_ie0clear` | 0 | IE0 clear -> de-assert immediately | VDP.cc:1196 `irqVertical.reset()` |
| `IV_ie0set2` | 1 | IE0 set again, F still pending -> re-assert | VDP.cc:1189-1194 |

## Interpretation

The `IV_ie0clear=0` edge is the M36 Bug B behavior: on real openMSX hardware,
clearing R#1 IE0 mid-frame IMMEDIATELY de-asserts a held VBlank /INT even though the
S#0 F flag is still latched. Our `change_register` reproduces exactly this (and the
`IV_ie0set*=1` re-assert-while-F-pending edges), which is the same sequence exercised
by `devices_v9958_ie0_register_write_irq_unit_test`. Before the fix our /INT stayed
asserted after the ISR cleared IE0, so the ISR's trailing `EI` re-entered forever
(the YS II interior-load interrupt storm / stack overflow).

