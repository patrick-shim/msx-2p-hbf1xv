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

// Suite: Devices_V9958CmdSlotFactor_Unit
// (M48 Slice 1, DEC-0075; docs/m48-planner-package.md §2.2/§3/§4.1 AC-1/AC-2/
//  AC-5 + EG-8 non-tautology)
//
// M48 Slice 1 replaces the empirical per-mode kActiveDisplaySlotFactorPercent
// placeholder with the principled, sprite-aware tier-1 slot-availability factor
// slot_factor = 154 / S_effective, where S_effective is one of the three
// fact-sheet §8-line-163 per-line access-slot COUNTS (154 display-off / 88
// sprites-off / 31 sprites-on). The armed S#2-bit0 CE busy window becomes
// ceil(base_duration * 154 / S_effective).
//
// EVERY expected value here is RE-DERIVED from the tier-1 constants
// (154/88/31, 1368, 6) and the hand-derived HMMV base duration (50), NEVER read
// back from the implementation (EG-8). Discrimination / non-tautology: the three
// regimes MUST produce three DISTINCT windows (50 / 88 / 249), so an adversarial
// mutation that (a) swaps 88<->31, (b) drops the factor (always 154), or (c)
// mis-decodes BL/SPD/mode makes a specific assertion FAIL -- documented at each
// case.
//
// Cases:
//   A  Header oracle: the three tier-1 slot COUNTS + slot_cost_tstates() are
//      re-derived from 1368/6 (ceil), with a floor-vs-ceil adversarial guard.
//   B1 Display OFF (raster<0) -> S=154 -> factor 1.00 -> window == base (50).
//      This is the A-M48-4 BYTE-IDENTITY regression vs v1.1.4 (the pre-M48
//      display-off path applied NO factor).
//   B2 Display OFF via BL=0 while the raster is in active display -> still 154
//      (a blanked screen frees all slots): proves BL gating (tier-1 correct;
//      the pre-M48 code wrongly inflated this because it keyed only on raster).
//   B3 Display ON + sprites OFF (SPD=1) -> S=88 -> ceil(50*154/88)=88.
//   B4 Display ON + sprites ON  (SPD=0) -> S=31 -> ceil(50*154/31)=249.
//   B5 Discrimination: the three windows are pairwise distinct (the factor is
//      genuinely 3-valued -- a dropped/collapsed factor fails here).
//   C  Mode exclusion: TEXT1 (no sprites) + CMD bit + SPD=0 in active display
//      uses the sprites-OFF factor (154/88), NOT sprites-on (154/31), because
//      text modes have no sprites (fact-sheet §6).
//   D  Determinism: identical inputs -> identical window across repeated runs.
//   E  --vdp-cmd-bias still applies AFTER the slot factor (default-0 knob
//      preserved; AC-5).

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_access_timing.h"

namespace {

using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpClockSource;
namespace vat = sony_msx::devices::video::vdp_access_timing;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

void expect_eq_u64(const std::uint64_t got, const std::uint64_t want, const char* case_name) {
    if (got != want) {
        std::cerr << "Case failed: " << case_name << " (got " << got << ", want " << want << ")\n";
        ++g_failures;
    }
}

// Independent (test-side) ceiling division -- NOT taken from the implementation.
constexpr std::uint64_t ceil_div(const std::uint64_t a, const std::uint64_t b) {
    return (a + b - 1) / b;
}

// Fully decoupled fake clock (same shape as the M44 CE-duration test): total_ is
// the ABSOLUTE monotonic count the CE window compares against; since_vsync_ feeds
// the raster derivation (raster_display_line() / S#2 VR/HR).
class FakeClock final : public VdpClockSource {
public:
    [[nodiscard]] std::uint64_t cpu_tstates_since_vsync() const override { return since_vsync_; }
    [[nodiscard]] std::uint64_t cpu_total_cycles() const override { return total_; }
    std::uint64_t total_ = 0;
    std::uint64_t since_vsync_ = 0;
};

void set_register(V9958Vdp& vdp, const std::uint8_t reg, const std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}
void set_cmd_reg(V9958Vdp& vdp, const int index, const std::uint8_t value) {
    set_register(vdp, static_cast<std::uint8_t>(32 + index), value);
}
void set_dx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 4, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 5, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_dy(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 6, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 7, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_nx(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 8, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 9, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_ny(V9958Vdp& vdp, const unsigned v) {
    set_cmd_reg(vdp, 10, static_cast<std::uint8_t>(v & 0xFF));
    set_cmd_reg(vdp, 11, static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void set_col(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 12, v); }
void set_cmd(V9958Vdp& vdp, const std::uint8_t v) { set_cmd_reg(vdp, 14, v); }

bool ce_bit(const V9958Vdp& vdp) { return (vdp.peek_status_register(2) & 0x01) != 0; }

// A raster position squarely inside the active display region for LN=0 (192
// lines): non_active_lines = 70, so line 100 -> raster_display_line() == 30 >= 0.
constexpr std::uint64_t kActiveVsync = 100ull * vat::kCpuTstatesPerLine;  // line 100
// A vblank raster (tstates==0 == first bottom-border line) -> raster < 0.
constexpr std::uint64_t kVblankVsync = 0;

// Measure the armed CE busy window: the smallest absolute cycle at which CE reads
// clear. Precondition: the command was issued (window armed) at clk.total_ == 0,
// so the returned value IS the window length in T-states. The engine's own ce()
// bit is already clear for an atomic HMMV, so this reads the window alone.
std::uint64_t measure_ce_window(V9958Vdp& vdp, FakeClock& clk) {
    for (std::uint64_t t = 0; t <= 500000; ++t) {
        clk.total_ = t;
        if (!ce_bit(vdp)) {
            return t;
        }
    }
    return 0xFFFFFFFFull;  // sentinel: never cleared (surfaces as a mismatch)
}

// GRAPHIC4 HMMV NX=6 NY=2 (base duration 50; see literal derivation below) under
// a chosen regime; returns the measured CE window.
std::uint64_t graphic4_hmmv_window(const std::uint64_t since_vsync, const std::uint8_t r1,
                                   const std::uint8_t r8) {
    V9958Vdp vdp;
    FakeClock clk;
    clk.total_ = 0;
    clk.since_vsync_ = since_vsync;
    vdp.attach_clock_source(&clk);
    set_register(vdp, 0, 0x06);   // GRAPHIC4 (base 0x0C)
    set_register(vdp, 1, r1);     // BL = bit6; keeps M1=M2=0 -> stays GRAPHIC4
    set_register(vdp, 8, r8);     // SPD = bit1
    clk.total_ = 0;               // arm at absolute 0
    set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
    set_cmd(vdp, 0xC0);           // HMMV -> arms the CE window
    return measure_ce_window(vdp, clk);
}

}  // namespace

int main() {
    // ================= A: header oracle (constants + slot_cost) ===============
    // Tier-1 slot COUNTS, cited fact-sheet §8 line 163 (154/88/31). Compile-time
    // proof plus a runtime re-derivation of slot_cost_tstates from 1368/6.
    static_assert(vat::kSlotsPerLineDisplayOff == 154);
    static_assert(vat::kSlotsPerLineSpritesOff == 88);
    static_assert(vat::kSlotsPerLineSpritesOn == 31);
    static_assert(vat::slot_cost_tstates(154) == 2);
    static_assert(vat::slot_cost_tstates(88) == 3);
    static_assert(vat::slot_cost_tstates(31) == 8);
    {
        expect_eq_u64(static_cast<std::uint64_t>(vat::kSlotsPerLineDisplayOff), 154,
                      "A_Const_DisplayOff_154");
        expect_eq_u64(static_cast<std::uint64_t>(vat::kSlotsPerLineSpritesOff), 88,
                      "A_Const_SpritesOff_88");
        expect_eq_u64(static_cast<std::uint64_t>(vat::kSlotsPerLineSpritesOn), 31,
                      "A_Const_SpritesOn_31");

        // slot_cost = ceil(1368 / (6 * S)) re-derived independently.
        expect_eq_u64(static_cast<std::uint64_t>(vat::slot_cost_tstates(154)), ceil_div(1368, 6 * 154),
                      "A_SlotCost_154_Rederived");
        expect_eq_u64(static_cast<std::uint64_t>(vat::slot_cost_tstates(88)), ceil_div(1368, 6 * 88),
                      "A_SlotCost_88_Rederived");
        expect_eq_u64(static_cast<std::uint64_t>(vat::slot_cost_tstates(31)), ceil_div(1368, 6 * 31),
                      "A_SlotCost_31_Rederived");
        expect_eq_u64(static_cast<std::uint64_t>(vat::slot_cost_tstates(154)), 2, "A_SlotCost_154_Is2");
        expect_eq_u64(static_cast<std::uint64_t>(vat::slot_cost_tstates(88)), 3, "A_SlotCost_88_Is3");
        expect_eq_u64(static_cast<std::uint64_t>(vat::slot_cost_tstates(31)), 8, "A_SlotCost_31_Is8");

        // Non-tautology (floor-vs-ceil guard): a floor mutation would give 88->2
        // and 31->7, and would break the strict ordering below.
        expect(vat::slot_cost_tstates(154) < vat::slot_cost_tstates(88) &&
                   vat::slot_cost_tstates(88) < vat::slot_cost_tstates(31),
               "A_SlotCost_StrictlyIncreasesAsSlotsShrink");
    }

    // Base-duration anchor (hand-derived, tier-1 §8 timing model): HMMV per-unit
    // 48, per-line 56. NX=6 -> tmp_nx=3 bytes (GRAPHIC4 = 2px/byte), NY=2 ->
    // tmp_ny=2. VDP cycles = 48*(3*2 - 1) + 56*(2 - 1) = 240 + 56 = 296;
    // base = ceil(296/6) = 50. This equals the engine's own pure duration.
    constexpr std::uint64_t kBase = 50;
    {
        V9958Vdp vdp;
        set_register(vdp, 0, 0x06);  // GRAPHIC4 (no clock -> pure duration only)
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);
        expect_eq_u64(vdp.cmd_engine().last_cmd_duration_tstates(), kBase, "Base_Hmmv_PureDuration_50");
        expect_eq_u64(kBase, ceil_div(296, 6), "Base_Hmmv_RederivedFrom296over6");
    }

    // ================= B1: display OFF -> byte-identity (factor 1.00) ==========
    // raster<0 (vblank). BL/SPD irrelevant. Window == base == 50 (== v1.1.4).
    const std::uint64_t win_off = graphic4_hmmv_window(kVblankVsync, /*r1=*/0x40, /*r8=*/0x00);
    expect_eq_u64(win_off, kBase, "B1_DisplayOff_Factor1_ByteIdenticalToV114");
    expect_eq_u64(win_off, ceil_div(kBase * 154, 154), "B1_DisplayOff_Rederived_154over154");

    // ================= B2: BL=0 in active display -> still 154 ================
    // Active raster, but BL=0 (blanked) -> display OFF -> S=154 -> factor 1.00.
    // (The pre-M48 code keyed only on raster and would have inflated this.)
    const std::uint64_t win_bl0 = graphic4_hmmv_window(kActiveVsync, /*r1=*/0x00, /*r8=*/0x00);
    expect_eq_u64(win_bl0, kBase, "B2_ActiveButBlanked_BL0_Factor1");

    // ================= B3: display ON + sprites OFF (SPD=1) -> S=88 ============
    const std::uint64_t win_spr_off = graphic4_hmmv_window(kActiveVsync, /*r1=*/0x40, /*r8=*/0x02);
    expect_eq_u64(win_spr_off, ceil_div(kBase * 154, 88), "B3_SpritesOff_154over88");
    expect_eq_u64(win_spr_off, 88, "B3_SpritesOff_Literal88");  // ceil(7700/88)=88

    // ================= B4: display ON + sprites ON (SPD=0) -> S=31 =============
    const std::uint64_t win_spr_on = graphic4_hmmv_window(kActiveVsync, /*r1=*/0x40, /*r8=*/0x00);
    expect_eq_u64(win_spr_on, ceil_div(kBase * 154, 31), "B4_SpritesOn_154over31");
    expect_eq_u64(win_spr_on, 249, "B4_SpritesOn_Literal249");  // ceil(7700/31)=249

    // ================= B5: discrimination (3 distinct windows) =================
    // The factor is genuinely 3-valued: 50 < 88 < 249. A mutation that swaps
    // 88<->31 breaks B3/B4; a mutation that drops the factor (always 154)
    // collapses all three to 50 and breaks B3/B4 + this.
    expect(win_off != win_spr_off && win_spr_off != win_spr_on && win_off != win_spr_on,
           "B5_ThreeRegimes_PairwiseDistinct");
    expect(win_off < win_spr_off && win_spr_off < win_spr_on, "B5_MonotonicInScarcity");

    // ================= C: mode exclusion (text has no sprites) ================
    // TEXT1 + CMD bit, SPD=0 (sprites "enabled") in active display MUST use the
    // sprites-OFF factor 154/88, because text modes have no sprites (§6). Measure
    // the text base once (raster<0 -> factor 1.00), then the active window.
    {
        std::uint64_t base_text = 0;
        // Text base (display OFF).
        {
            V9958Vdp vdp;
            FakeClock clk;
            clk.total_ = 0;
            clk.since_vsync_ = kVblankVsync;
            vdp.attach_clock_source(&clk);
            set_register(vdp, 0, 0x00);
            set_register(vdp, 1, 0x10);   // M1=1 -> TEXT1 (BL=0 here)
            set_register(vdp, 25, 0x40);  // CMD bit: commands legal in non-bitmap modes
            set_register(vdp, 8, 0x00);   // SPD=0
            clk.total_ = 0;
            set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 64); set_ny(vdp, 4); set_col(vdp, 0x5A);
            set_cmd(vdp, 0xC0);
            base_text = vdp.cmd_engine().last_cmd_duration_tstates();
            const std::uint64_t win_text_off = measure_ce_window(vdp, clk);
            expect(base_text > 0, "C_Text_BaseDuration_NonZero");
            expect_eq_u64(win_text_off, base_text, "C_Text_DisplayOff_Factor1");
        }
        // Text active display, SPD=0 -> sprites-off factor (no sprites in text).
        {
            V9958Vdp vdp;
            FakeClock clk;
            clk.total_ = 0;
            clk.since_vsync_ = kActiveVsync;
            vdp.attach_clock_source(&clk);
            set_register(vdp, 0, 0x00);
            set_register(vdp, 1, 0x50);   // BL=1 + M1=1 -> TEXT1, display on
            set_register(vdp, 25, 0x40);  // CMD bit
            set_register(vdp, 8, 0x00);   // SPD=0 (sprites "enabled" -- but text has none)
            clk.total_ = 0;
            set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 64); set_ny(vdp, 4); set_col(vdp, 0x5A);
            set_cmd(vdp, 0xC0);
            const std::uint64_t win_text_active = measure_ce_window(vdp, clk);
            expect_eq_u64(win_text_active, ceil_div(base_text * 154, 88),
                          "C_Text_ActiveSpd0_UsesSpritesOff88");
            expect(win_text_active != ceil_div(base_text * 154, 31),
                   "C_Text_NotSpritesOn31");  // fails if text is treated as sprites-on
        }
    }

    // ================= D: determinism =========================================
    {
        const std::uint64_t a = graphic4_hmmv_window(kActiveVsync, 0x40, 0x00);
        const std::uint64_t b = graphic4_hmmv_window(kActiveVsync, 0x40, 0x00);
        expect_eq_u64(a, b, "D_Determinism_IdenticalInputsIdenticalWindow");
        expect_eq_u64(a, win_spr_on, "D_Determinism_MatchesEarlierRun");
    }

    // ================= E: --vdp-cmd-bias applied AFTER the factor ==============
    {
        V9958Vdp vdp;
        FakeClock clk;
        clk.total_ = 0;
        clk.since_vsync_ = kActiveVsync;
        vdp.attach_clock_source(&clk);
        set_register(vdp, 0, 0x06);  // GRAPHIC4
        set_register(vdp, 1, 0x40);  // BL=1
        set_register(vdp, 8, 0x00);  // SPD=0 -> sprites on -> factor 154/31
        vdp.set_cmd_busy_bias(10);
        clk.total_ = 0;
        set_dx(vdp, 0); set_dy(vdp, 0); set_nx(vdp, 6); set_ny(vdp, 2); set_col(vdp, 0x5A);
        set_cmd(vdp, 0xC0);
        const std::uint64_t w = measure_ce_window(vdp, clk);
        // Sprites-on window (249) + bias (10) = 259: the bias is added AFTER the
        // slot factor (AC-5), not before.
        expect_eq_u64(w, ceil_div(kBase * 154, 31) + 10, "E_Bias_AppliedAfterFactor");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
