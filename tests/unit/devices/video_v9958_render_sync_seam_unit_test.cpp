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

// Suite: Devices_V9958RenderSyncSeam_Unit (M32-S1, Defect A of DEC-0039,
// docs/m32-planner-package.md §3-S1 / test matrix row 2)
//
// The nullable VdpRenderSyncListener seam on V9958Vdp (§2.3):
//   1. Null listener (the default) => byte-identical VDP behavior --
//      registers, VRAM, status, palette, and the rendered frame of a
//      listener-attached VDP driven by a counting no-op listener match an
//      untouched twin exactly (the seam itself perturbs nothing).
//   2. The listener is called BEFORE state mutates -- old-state visibility
//      at callback time for register commits AND #98 VRAM data writes
//      (the openMSX sync-before-change protocol,
//      references/openmsx-21.0/src/video/PixelRenderer.cc:253-394/510-517).
//   3. All four ports (#98/#99/#9A/#9B) invoke the hook, exactly once per
//      io_write; io_read never invokes it; detaching (nullptr) stops it.
//
// (The machine-level `debug_io_write` hook-exclusion property is proven in
// tests/integration/machine/hbf1xv_m32_per_line_latch_integration_test.cpp
// -- debug_io_write is a Hbf1xvMachine seam, not a V9958Vdp one.)

#include <cstdint>
#include <iostream>

#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_frame_renderer.h"

namespace {

using sony_msx::devices::video::Field;
using sony_msx::devices::video::V9958Vdp;
using sony_msx::devices::video::VdpFrameRenderer;
using sony_msx::devices::video::VdpRenderSyncListener;

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Counting listener that snapshots chosen VDP state at callback time --
// read-only, mirroring the §2.3 contract (a real render-sync listener only
// reads registers/VRAM and writes its own pixel store).
class RecordingListener final : public VdpRenderSyncListener {
public:
    explicit RecordingListener(const V9958Vdp& vdp) : vdp_(&vdp) {}

    void on_before_state_change() override {
        ++calls;
        last_r7_at_callback = vdp_->control_register(7);
        last_vram0_at_callback = vdp_->vram().read(0);
    }

    int calls = 0;
    std::uint8_t last_r7_at_callback = 0xEE;
    std::uint8_t last_vram0_at_callback = 0xEE;

private:
    const V9958Vdp* vdp_;
};

void set_register(V9958Vdp& vdp, const std::uint8_t reg, const std::uint8_t value) {
    vdp.io_write(0x99, value);
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x80 | (reg & 0x3F)));
}

void set_write_address(V9958Vdp& vdp, const std::uint16_t addr) {
    vdp.io_write(0x99, static_cast<std::uint8_t>(addr & 0xFF));
    vdp.io_write(0x99, static_cast<std::uint8_t>(0x40 | ((addr >> 8) & 0x3F)));
}

}  // namespace

int main() {
    // --- 1. Null-listener byte-identity: an attached counting listener
    //     perturbs NOTHING -- drive identical sequences on a plain VDP and
    //     a listener-attached VDP; every observable matches. ---
    {
        V9958Vdp plain;
        V9958Vdp hooked;
        RecordingListener listener(hooked);
        hooked.attach_render_sync(&listener);

        const auto drive = [](V9958Vdp& vdp) {
            set_register(vdp, 0, 0x06);   // GRAPHIC4
            set_register(vdp, 7, 0x4C);
            set_register(vdp, 23, 17);
            set_write_address(vdp, 0x0100);
            for (int i = 0; i < 64; ++i) {
                vdp.io_write(0x98, static_cast<std::uint8_t>(0xA0 + i));
            }
            set_register(vdp, 16, 3);
            vdp.io_write(0x9A, 0x35);     // palette two-write
            vdp.io_write(0x9A, 0x06);
            set_register(vdp, 17, 26);
            vdp.io_write(0x9B, 0x05);     // indirect write to R#26
        };
        drive(plain);
        drive(hooked);

        bool state_identical = true;
        for (int reg = 0; reg < V9958Vdp::kNumControlRegs; ++reg) {
            if (plain.control_register(reg) != hooked.control_register(reg)) {
                state_identical = false;
            }
        }
        for (int i = 0; i < 0x200; ++i) {
            if (plain.vram().read(static_cast<std::uint32_t>(i)) !=
                hooked.vram().read(static_cast<std::uint32_t>(i))) {
                state_identical = false;
            }
        }
        for (int s = 0; s <= 9; ++s) {
            if (plain.peek_status_register(s) != hooked.peek_status_register(s)) {
                state_identical = false;
            }
        }
        for (int p = 0; p < 16; ++p) {
            if (plain.palette_entry(p) != hooked.palette_entry(p)) {
                state_identical = false;
            }
        }
        expect(state_identical, "AttachedListener_VdpState_ByteIdenticalToPlainTwin");

        const VdpFrameRenderer rp(plain);
        const VdpFrameRenderer rh(hooked);
        expect(rp.render_frame(Field::Progressive).pixels == rh.render_frame(Field::Progressive).pixels,
               "AttachedListener_RenderedFrame_ByteIdenticalToPlainTwin");
        expect(listener.calls > 0, "AttachedListener_HookActuallyFired");
    }

    // --- 2. Old-state visibility: the hook runs BEFORE the write mutates.
    //     R#7 commit: callback during the committing #99 write still sees
    //     the OLD R#7; VRAM: callback during the #98 write sees the OLD
    //     byte at the target address. ---
    {
        V9958Vdp vdp;
        RecordingListener listener(vdp);
        vdp.attach_render_sync(&listener);

        set_register(vdp, 7, 0x11);
        expect(vdp.control_register(7) == 0x11, "Setup_R7Committed");
        // Change R#7 0x11 -> 0x22; at BOTH hook invocations of the two-write
        // protocol the visible R#7 must still be 0x11.
        vdp.io_write(0x99, 0x22);
        expect(listener.last_r7_at_callback == 0x11, "Hook_BeforeDataLatchWrite_SeesOldR7");
        vdp.io_write(0x99, 0x80 | 7);
        expect(listener.last_r7_at_callback == 0x11, "Hook_BeforeRegisterCommit_SeesOldR7");
        expect(vdp.control_register(7) == 0x22, "Commit_AfterHook_NewR7Visible");

        set_write_address(vdp, 0);
        vdp.io_write(0x98, 0x5A);
        expect(listener.last_vram0_at_callback == 0x00, "Hook_BeforeVramDataWrite_SeesOldByte");
        expect(vdp.vram().read(0) == 0x5A, "VramWrite_AfterHook_NewByteVisible");
    }

    // --- 3. Coverage/count: one hook call per io_write on each of the four
    //     ports; io_read never fires it; nullptr detaches. ---
    {
        V9958Vdp vdp;
        RecordingListener listener(vdp);
        vdp.attach_render_sync(&listener);

        vdp.io_write(0x98, 0x00);
        expect(listener.calls == 1, "Port98_OneCallPerWrite");
        vdp.io_write(0x99, 0x00);
        vdp.io_write(0x99, 0x87);
        expect(listener.calls == 3, "Port99_OneCallPerWrite_TwoWriteProtocol");
        vdp.io_write(0x9A, 0x00);
        expect(listener.calls == 4, "Port9A_OneCallPerWrite");
        vdp.io_write(0x9B, 0x00);
        expect(listener.calls == 5, "Port9B_OneCallPerWrite");

        (void)vdp.io_read(0x98);
        (void)vdp.io_read(0x99);
        (void)vdp.io_read(0x9A);
        expect(listener.calls == 5, "IoRead_NeverFiresHook");

        vdp.attach_render_sync(nullptr);
        vdp.io_write(0x98, 0x01);
        expect(listener.calls == 5, "DetachNullptr_HookStops");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_V9958RenderSyncSeam_Unit cases passed\n";
    return 0;
}
