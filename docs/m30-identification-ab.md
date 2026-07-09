# M30-S6 openMSX A/B Check -- Cartridge Auto-Identification Agreement

- Milestone: M30 (universal cartridge auto-identification, backlog G2), slice S6.
- Subject-A emulator: `sony_msx_headless` (this repo, Debug build).
- Reference-B emulator: openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC (WSL, `/usr/bin/openmsx`), machine `Sony_HB-F1XV`.
- ROM under test: `roms/aleste.rom` (local dev asset; SHA1 `e93d0840c59c6eba273df546d22148d486a150a6`).
- NEITHER side forces a mapper type: Side A has NO `--cart1-type`; Side B has NO `-romtype`.

## Why this is the ONE meaningful A/B check (planner section 2.5)

Identification is frontend/composition POLICY (a pure function of file bytes + database
bytes), not device behavior -- there is no hardware register to compare, so per-register
A/B would be fabricated relevance. The meaningful cross-emulator fact is the RESOLVED
mapper type for the same input file with no forcing on either side.

## R-M30-6: Side-B Tcl query mechanism -- VERIFIED IN SOURCE FIRST

- `references/openmsx-21.0/src/MSXMotherBoard.cc:1227-1252` -- `machine_info device <name>` -> `device->getDeviceInfo(result)`.
- `references/openmsx-21.0/src/memory/MSXRom.cc:56-63` -- `getExtraDeviceInfo` -> `getInfo` -> dict keys `mappertype` (the RESOLVED type; MSXRom.cc:26-27: *'auto' is already changed to actual type*) + `actualSHA1` (our cartridge-device selector -- system BIOS/SubROM/Kanji ROMs are MSXRom devices too and also report a mappertype).
- `references/openmsx-21.0/src/config/HardwareConfig.cc:115-122` -- with no `-romtype`, the generated cart config's `<mappertype>` is `auto`.
- `references/openmsx-21.0/src/memory/RomFactory.cc:176-219` -- `auto` resolves via openMSX's own softwaredb SHA1 lookup, and the RESOLVED type is written back into the config the query reads.

## Side A (this emulator, NO --cart1-type)

```
$ build/Debug/sony_msx_headless.exe --cart1 roms/aleste.rom
cartridge: --cart1: identified as "Aleste 2" (KonamiSCC) via softwaredb SHA1 match [sha1=e93d0840c59c6eba273df546d22148d486a150a6, db=references/openmsx-21.0/share/softwaredb.xml]
cartridge: --cart1 loaded (roms/aleste.rom, KonamiSCC)
sony-msx-hbf1xv headless scaffold
elapsed_cycles=59736
frame_count=1
frame_cycles_per_frame=59736
exit code: 0
```

## Side B (openMSX WSL, -carta with NO -romtype; live Tcl query)

```
OMIDENT mappertype=Mirrored sha1=174c9254f09d99361ff7607630248ff9d7d8d4d6 device=MSX BIOS with BASIC ROM
OMIDENT mappertype=Halnote sha1=ade0c5ba5574f8114d7079050317099b4519e88f device=HB-F1XV MSX-JE
OMIDENT mappertype=Mirrored sha1=fe0254cbfc11405b79e7c86c7769bd6322b04995 device=MSX Sub ROM
OMIDENT mappertype=Mirrored sha1=dcc3a67732aa01c4f2ee8d1ad886444a4dbafe06 device=MSX Kanji Driver with BASIC
OMIDENT mappertype=KonamiSCC sha1=e93d0840c59c6eba273df546d22148d486a150a6 device=Aleste 2
```

Cartridge device selected by actualSHA1 == `e93d0840c59c6eba273df546d22148d486a150a6`: device `Aleste 2`, mappertype `KonamiSCC`.

## Result

- **Result: AGREEMENT.** Side A resolved **KonamiSCC** (via softwaredb SHA1 match, title `Aleste 2`); Side B resolved **KonamiSCC** (openMSX's own softwaredb `auto` resolution). Both sides identify the same file as the same mapper type with zero forcing.

## Reproduce

```powershell
cmake --build build --config Debug
tools/openmsx-m30-identification-ab.ps1
```

