# FM-PAC Firmware and SRAM Directory

This directory is reserved for user-supplied FM-PAC firmware and
associated SRAM data required for local emulator operation and testing.

## Third-Party Firmware

The FM-PAC firmware is third-party proprietary software and is not
included, distributed, licensed, or sublicensed by this project.

Users must provide any required FM-PAC firmware separately and are
solely responsible for obtaining, possessing, copying, and using it
lawfully and in accordance with applicable law and any licence terms
imposed by the relevant rights holders.

References to firmware filenames, versions, sizes, memory layouts, or
cryptographic hashes are provided solely for compatibility
identification and reproducible testing. Such references do not include
the underlying firmware and do not grant any right to obtain, copy,
modify, or redistribute it.

No licence, ownership interest, authorization, waiver, endorsement, or
other right in the FM-PAC firmware or any other third-party intellectual
property is granted, expressed, or implied by this project.

## Local File Placement

For local use, place the required files in this directory using the
filenames expected by the emulator:

roms/
├── fmpac.rom
└── fmpac.rom.sram