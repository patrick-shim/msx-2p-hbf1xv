// ============================================================================
//  Sony HB-F1XV MSX2+ Emulator
//  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
//
//  LEGAL NOTICE - Personal reference only.
//  This source code is made available solely for personal, non-commercial
//  reference and educational study. Commercial use, sale, or redistribution
//  for profit is not permitted without the author's written consent.
//  Provided "AS IS", without warranty of any kind.
//  Proprietary BIOS/ROM/disk assets remain the property of their respective
//  rights holders and are NOT licensed by this notice.
// ============================================================================

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

#include "machine/cpu_trace_sink.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvParityCheckpoint_Integration
//
// Regression-locks THIS emulator's half of the M10-S4 openMSX opcode-trace
// parity harness. The committed RAM-only, I/O-free parity program
// (tests/parity/z80_parity_checkpoint.bin) is loaded at its documented base
// (0xC000) from this emulator's cold_boot reset vector; the deterministic
// M10-S1 trace-export is captured to the checkpoint (HALT at 0xC055). The
// serialized trace is asserted byte-for-byte against a committed golden so the
// emulator side of the parity diff cannot silently drift, independent of the
// WSL/openMSX environment. The openMSX side and the actual empty architectural
// diff are captured by tools/openmsx-trace-parity.ps1 into
// docs/m10-parity-trace-diff.md.

namespace {

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Byte-identical copy of tests/parity/z80_parity_checkpoint.bin (95 bytes).
// SHA-256 b58ccb3e84e94a20d771a0dce171bc1426fbe50a5f93505c6afbe54cdca732db.
const std::array<std::uint8_t, 95> kParityProgram{
    0xF3, 0xED, 0x56, 0x31, 0xF0, 0xFF, 0x3E, 0x2A, 0x06, 0x03, 0x0E, 0x05,
    0x80, 0x91, 0x0F, 0xC6, 0x07, 0xE6, 0x3F, 0xF6, 0x40, 0xEE, 0xFF, 0xFE,
    0xA4, 0x3C, 0x3D, 0x47, 0xCB, 0x40, 0xCB, 0x00, 0xCB, 0x38, 0x21, 0x00,
    0xC2, 0x11, 0x20, 0xC2, 0x36, 0x11, 0x23, 0x36, 0x22, 0x2B, 0x01, 0x01,
    0x00, 0xED, 0xA0, 0xDD, 0x21, 0x00, 0xC2, 0xDD, 0x7E, 0x00, 0xDD, 0x34,
    0x01, 0xDD, 0x86, 0x00, 0xFD, 0x21, 0x40, 0xC2, 0xFD, 0x36, 0x00, 0x55,
    0xFD, 0x7E, 0x00, 0xCD, 0x5C, 0xC0, 0x06, 0x03, 0x0C, 0x10, 0xFD, 0x18,
    0x00, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x04, 0xC9,
};

constexpr std::uint16_t kBase = 0xC000;
constexpr std::uint16_t kCheckpointPc = 0xC055;

// Committed golden: the exact deterministic CpuTraceSink serialization this
// emulator produces for the parity program to the checkpoint.
const char* kGoldenTrace =
        "SEQ=0000 PC=C000 OP=F3 A=00 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=0000 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=00 IFF1=0 IFF2=0 IM=1 T=4 TC=4\n"
        "SEQ=0001 PC=C001 OP=ED56 A=00 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=0000 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=01 IFF1=0 IFF2=0 IM=1 T=8 TC=12\n"
        "SEQ=0002 PC=C003 OP=31 A=00 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=0000 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=03 IFF1=0 IFF2=0 IM=1 T=10 TC=22\n"
        "SEQ=0003 PC=C006 OP=3E A=00 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=0000 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=04 IFF1=0 IFF2=0 IM=1 T=7 TC=29\n"
        "SEQ=0004 PC=C008 OP=06 A=2A F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=2A00 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=05 IFF1=0 IFF2=0 IM=1 T=7 TC=36\n"
        "SEQ=0005 PC=C00A OP=0E A=2A F=00[........] B=03 C=00 D=00 E=00 H=00 L=00 AF=2A00 BC=0300 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=06 IFF1=0 IFF2=0 IM=1 T=7 TC=43\n"
        "SEQ=0006 PC=C00C OP=80 A=2A F=00[........] B=03 C=05 D=00 E=00 H=00 L=00 AF=2A00 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=07 IFF1=0 IFF2=0 IM=1 T=4 TC=47\n"
        "SEQ=0007 PC=C00D OP=91 A=2D F=28[..Y.X...] B=03 C=05 D=00 E=00 H=00 L=00 AF=2D28 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=08 IFF1=0 IFF2=0 IM=1 T=4 TC=51\n"
        "SEQ=0008 PC=C00E OP=0F A=28 F=2A[..Y.X.N.] B=03 C=05 D=00 E=00 H=00 L=00 AF=282A BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=09 IFF1=0 IFF2=0 IM=1 T=4 TC=55\n"
        "SEQ=0009 PC=C00F OP=C6 A=14 F=00[........] B=03 C=05 D=00 E=00 H=00 L=00 AF=1400 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=0A IFF1=0 IFF2=0 IM=1 T=7 TC=62\n"
        "SEQ=000A PC=C011 OP=E6 A=1B F=08[....X...] B=03 C=05 D=00 E=00 H=00 L=00 AF=1B08 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=0B IFF1=0 IFF2=0 IM=1 T=7 TC=69\n"
        "SEQ=000B PC=C013 OP=F6 A=1B F=1C[...HXP..] B=03 C=05 D=00 E=00 H=00 L=00 AF=1B1C BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=0C IFF1=0 IFF2=0 IM=1 T=7 TC=76\n"
        "SEQ=000C PC=C015 OP=EE A=5B F=08[....X...] B=03 C=05 D=00 E=00 H=00 L=00 AF=5B08 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=0D IFF1=0 IFF2=0 IM=1 T=7 TC=83\n"
        "SEQ=000D PC=C017 OP=FE A=A4 F=A0[S.Y.....] B=03 C=05 D=00 E=00 H=00 L=00 AF=A4A0 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=0E IFF1=0 IFF2=0 IM=1 T=7 TC=90\n"
        "SEQ=000E PC=C019 OP=3C A=A4 F=62[.ZY...N.] B=03 C=05 D=00 E=00 H=00 L=00 AF=A462 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=0F IFF1=0 IFF2=0 IM=1 T=4 TC=94\n"
        "SEQ=000F PC=C01A OP=3D A=A5 F=A0[S.Y.....] B=03 C=05 D=00 E=00 H=00 L=00 AF=A5A0 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=10 IFF1=0 IFF2=0 IM=1 T=4 TC=98\n"
        "SEQ=0010 PC=C01B OP=47 A=A4 F=A2[S.Y...N.] B=03 C=05 D=00 E=00 H=00 L=00 AF=A4A2 BC=0305 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=11 IFF1=0 IFF2=0 IM=1 T=4 TC=102\n"
        "SEQ=0011 PC=C01C OP=CB40 A=A4 F=A2[S.Y...N.] B=A4 C=05 D=00 E=00 H=00 L=00 AF=A4A2 BC=A405 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=12 IFF1=0 IFF2=0 IM=1 T=8 TC=110\n"
        "SEQ=0012 PC=C01E OP=CB00 A=A4 F=74[.ZYH.P..] B=A4 C=05 D=00 E=00 H=00 L=00 AF=A474 BC=A405 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=14 IFF1=0 IFF2=0 IM=1 T=8 TC=118\n"
        "SEQ=0013 PC=C020 OP=CB38 A=A4 F=09[....X..C] B=49 C=05 D=00 E=00 H=00 L=00 AF=A409 BC=4905 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=16 IFF1=0 IFF2=0 IM=1 T=8 TC=126\n"
        "SEQ=0014 PC=C022 OP=21 A=A4 F=25[..Y..P.C] B=24 C=05 D=00 E=00 H=00 L=00 AF=A425 BC=2405 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=18 IFF1=0 IFF2=0 IM=1 T=10 TC=136\n"
        "SEQ=0015 PC=C025 OP=11 A=A4 F=25[..Y..P.C] B=24 C=05 D=00 E=00 H=C2 L=00 AF=A425 BC=2405 DE=0000 HL=C200 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=19 IFF1=0 IFF2=0 IM=1 T=10 TC=146\n"
        "SEQ=0016 PC=C028 OP=36 A=A4 F=25[..Y..P.C] B=24 C=05 D=C2 E=20 H=C2 L=00 AF=A425 BC=2405 DE=C220 HL=C200 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=1A IFF1=0 IFF2=0 IM=1 T=10 TC=156\n"
        "SEQ=0017 PC=C02A OP=23 A=A4 F=25[..Y..P.C] B=24 C=05 D=C2 E=20 H=C2 L=00 AF=A425 BC=2405 DE=C220 HL=C200 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=1B IFF1=0 IFF2=0 IM=1 T=6 TC=162\n"
        "SEQ=0018 PC=C02B OP=36 A=A4 F=25[..Y..P.C] B=24 C=05 D=C2 E=20 H=C2 L=01 AF=A425 BC=2405 DE=C220 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=1C IFF1=0 IFF2=0 IM=1 T=10 TC=172\n"
        "SEQ=0019 PC=C02D OP=2B A=A4 F=25[..Y..P.C] B=24 C=05 D=C2 E=20 H=C2 L=01 AF=A425 BC=2405 DE=C220 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=1D IFF1=0 IFF2=0 IM=1 T=6 TC=178\n"
        "SEQ=001A PC=C02E OP=01 A=A4 F=25[..Y..P.C] B=24 C=05 D=C2 E=20 H=C2 L=00 AF=A425 BC=2405 DE=C220 HL=C200 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=1E IFF1=0 IFF2=0 IM=1 T=10 TC=188\n"
        "SEQ=001B PC=C031 OP=EDA0 A=A4 F=25[..Y..P.C] B=00 C=01 D=C2 E=20 H=C2 L=00 AF=A425 BC=0001 DE=C220 HL=C200 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=1F IFF1=0 IFF2=0 IM=1 T=16 TC=204\n"
        "SEQ=001C PC=C033 OP=DD21 A=A4 F=01[.......C] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=A401 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFF0 I=00 R=21 IFF1=0 IFF2=0 IM=1 T=14 TC=218\n"
        "SEQ=001D PC=C037 OP=DD7E A=A4 F=01[.......C] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=A401 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=0000 SP=FFF0 I=00 R=23 IFF1=0 IFF2=0 IM=1 T=19 TC=237\n"
        "SEQ=001E PC=C03A OP=DD34 A=11 F=01[.......C] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=1101 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=0000 SP=FFF0 I=00 R=25 IFF1=0 IFF2=0 IM=1 T=23 TC=260\n"
        "SEQ=001F PC=C03D OP=DD86 A=11 F=21[..Y....C] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=1121 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=0000 SP=FFF0 I=00 R=27 IFF1=0 IFF2=0 IM=1 T=19 TC=279\n"
        "SEQ=0020 PC=C040 OP=FD21 A=22 F=20[..Y.....] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=2220 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=0000 SP=FFF0 I=00 R=29 IFF1=0 IFF2=0 IM=1 T=14 TC=293\n"
        "SEQ=0021 PC=C044 OP=FD36 A=22 F=20[..Y.....] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=2220 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=2B IFF1=0 IFF2=0 IM=1 T=19 TC=312\n"
        "SEQ=0022 PC=C048 OP=FD7E A=22 F=20[..Y.....] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=2220 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=2D IFF1=0 IFF2=0 IM=1 T=19 TC=331\n"
        "SEQ=0023 PC=C04B OP=CD A=55 F=20[..Y.....] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=5520 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=2F IFF1=0 IFF2=0 IM=1 T=17 TC=348\n"
        "SEQ=0024 PC=C05C OP=47 A=55 F=20[..Y.....] B=00 C=00 D=C2 E=21 H=C2 L=01 AF=5520 BC=0000 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFEE I=00 R=30 IFF1=0 IFF2=0 IM=1 T=4 TC=352\n"
        "SEQ=0025 PC=C05D OP=04 A=55 F=20[..Y.....] B=55 C=00 D=C2 E=21 H=C2 L=01 AF=5520 BC=5500 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFEE I=00 R=31 IFF1=0 IFF2=0 IM=1 T=4 TC=356\n"
        "SEQ=0026 PC=C05E OP=C9 A=55 F=00[........] B=56 C=00 D=C2 E=21 H=C2 L=01 AF=5500 BC=5600 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFEE I=00 R=32 IFF1=0 IFF2=0 IM=1 T=10 TC=366\n"
        "SEQ=0027 PC=C04E OP=06 A=55 F=00[........] B=56 C=00 D=C2 E=21 H=C2 L=01 AF=5500 BC=5600 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=33 IFF1=0 IFF2=0 IM=1 T=7 TC=373\n"
        "SEQ=0028 PC=C050 OP=0C A=55 F=00[........] B=03 C=00 D=C2 E=21 H=C2 L=01 AF=5500 BC=0300 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=34 IFF1=0 IFF2=0 IM=1 T=4 TC=377\n"
        "SEQ=0029 PC=C051 OP=10 A=55 F=00[........] B=03 C=01 D=C2 E=21 H=C2 L=01 AF=5500 BC=0301 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=35 IFF1=0 IFF2=0 IM=1 T=13 TC=390\n"
        "SEQ=002A PC=C050 OP=0C A=55 F=00[........] B=02 C=01 D=C2 E=21 H=C2 L=01 AF=5500 BC=0201 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=36 IFF1=0 IFF2=0 IM=1 T=4 TC=394\n"
        "SEQ=002B PC=C051 OP=10 A=55 F=00[........] B=02 C=02 D=C2 E=21 H=C2 L=01 AF=5500 BC=0202 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=37 IFF1=0 IFF2=0 IM=1 T=13 TC=407\n"
        "SEQ=002C PC=C050 OP=0C A=55 F=00[........] B=01 C=02 D=C2 E=21 H=C2 L=01 AF=5500 BC=0102 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=38 IFF1=0 IFF2=0 IM=1 T=4 TC=411\n"
        "SEQ=002D PC=C051 OP=10 A=55 F=00[........] B=01 C=03 D=C2 E=21 H=C2 L=01 AF=5500 BC=0103 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=39 IFF1=0 IFF2=0 IM=1 T=8 TC=419\n"
        "SEQ=002E PC=C053 OP=18 A=55 F=00[........] B=00 C=03 D=C2 E=21 H=C2 L=01 AF=5500 BC=0003 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=3A IFF1=0 IFF2=0 IM=1 T=12 TC=431\n"
        "SEQ=002F PC=C055 OP=76 A=55 F=00[........] B=00 C=03 D=C2 E=21 H=C2 L=01 AF=5500 BC=0003 DE=C221 HL=C201 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=C200 IY=C240 SP=FFF0 I=00 R=3B IFF1=0 IFF2=0 IM=1 T=4 TC=435\n";

std::string run_parity_trace() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM at 0xC000 (page 3); page flat 64K RAM (M13-S4)
    machine.load_memory(kBase, kParityProgram.data(),
                        static_cast<std::uint32_t>(kParityProgram.size()));
    machine.cpu().state().regs().pc = kBase;
    machine.set_cpu_trace_enabled(true);
    for (int i = 0; i < 256 && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
    return machine.cpu_trace().serialize();
}

}  // namespace

int main() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM at 0xC000 (page 3); page flat 64K RAM (M13-S4)
    machine.load_memory(kBase, kParityProgram.data(),
                        static_cast<std::uint32_t>(kParityProgram.size()));
    machine.cpu().state().regs().pc = kBase;
    machine.set_cpu_trace_enabled(true);

    int steps = 0;
    for (; steps < 256 && !machine.cpu().state().halted(); ++steps) {
        machine.step_cpu_instruction();
    }

    if (!expect_true(machine.cpu().state().halted(),
                     "ParityProgram_RunToCheckpoint_Halts")) {
        return 1;
    }
    if (!expect_true(machine.cpu_trace().size() == 48,
                     "ParityProgram_InstructionCount_Is48")) {
        std::cerr << "  size=" << machine.cpu_trace().size() << "\n";
        return 1;
    }

    const auto& last = machine.cpu_trace().at(machine.cpu_trace().size() - 1);
    if (!expect_true(last.pc == kCheckpointPc && last.opcode_length == 1 &&
                         last.opcode_bytes[0] == 0x76,
                     "ParityProgram_FinalInstruction_IsHaltAtCheckpoint")) {
        return 1;
    }
    if (!expect_true(last.cumulative_tstates == 435,
                     "ParityProgram_FinalCumulativeTstates_Is435")) {
        std::cerr << "  tc=" << last.cumulative_tstates << "\n";
        return 1;
    }

    const std::string serialized = machine.cpu_trace().serialize();
    if (!expect_true(serialized == std::string(kGoldenTrace),
                     "ParityProgram_Serialization_MatchesCommittedGolden")) {
        return 1;
    }

    // Determinism: a second independent run is byte-for-byte identical.
    const std::string run_b = run_parity_trace();
    if (!expect_true(run_b == serialized,
                     "ParityProgram_TwoRuns_ByteForByteIdentical")) {
        return 1;
    }

    return 0;
}
