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

// Suite: Frontend_DialogMailbox_Unit (M56, DEC-0084, planner §9 test 1; slice S2).
//
// The SDL-free classification of an SDL file-dialog callback's filelist, proven
// headlessly (links sony_msx_core, registers OUTSIDE the SONY_MSX_ENABLE_SDL3
// guard). The SDL_Mutex mechanics are exercised by the SDL3-gated F1 tests; here
// the pure decision logic is nailed down:
//   * filelist == NULL      -> Error
//   * filelist[0] == NULL   -> Cancel
//   * filelist[0] != NULL   -> Selection with the right count / order (deep copy)
//   * the drain no-ops on Error / Cancel / empty selection.

#include <iostream>
#include <string>
#include <vector>

#include "frontend/dialog_result.h"

namespace {

using namespace sony_msx::frontend;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// A local re-expression of the callback's selection deep-copy loop
// (sdl3_app.cpp Sdl3App::dialog_callback) so the ORDER/COUNT contract is proven
// SDL-free: copy every non-null path in order.
std::vector<std::string> deep_copy_selection(const std::vector<const char*>& filelist) {
    std::vector<std::string> out;
    for (const char* p : filelist) {
        if (p == nullptr) {
            break;
        }
        out.emplace_back(p);
    }
    return out;
}

// A local re-expression of the drain gate (drain_dialog_mailbox): apply only on a
// Selection with a non-empty path list.
bool drain_would_apply(const DialogResultKind kind, const std::size_t path_count) {
    if (kind == DialogResultKind::Error || kind == DialogResultKind::Cancel) {
        return false;
    }
    return path_count > 0;
}

}  // namespace

int main() {
    // --- Classification. ---
    expect(classify_dialog_filelist(/*filelist_null=*/true, /*first_null=*/false) ==
               DialogResultKind::Error,
           "Classify_NullFilelist_Error");
    // first_null is ignored when filelist_null is true (error dominates).
    expect(classify_dialog_filelist(true, true) == DialogResultKind::Error,
           "Classify_NullFilelist_ErrorDominates");
    expect(classify_dialog_filelist(false, /*first_null=*/true) == DialogResultKind::Cancel,
           "Classify_FirstNull_Cancel");
    expect(classify_dialog_filelist(false, /*first_null=*/false) == DialogResultKind::Selection,
           "Classify_NonNull_Selection");

    // --- Deep-copy preserves order + count (multi-select). ---
    {
        const std::vector<const char*> fl = {"d1.dsk", "d2.dsk", "save.dsk", nullptr};
        const std::vector<std::string> got = deep_copy_selection(fl);
        expect(got.size() == 3, "DeepCopy_CountThree");
        expect(got[0] == "d1.dsk" && got[1] == "d2.dsk" && got[2] == "save.dsk",
               "DeepCopy_OrderPreserved");
    }
    {
        const std::vector<const char*> fl = {"only.dsk", nullptr};
        const std::vector<std::string> got = deep_copy_selection(fl);
        expect(got.size() == 1 && got[0] == "only.dsk", "DeepCopy_SingleSelection");
    }

    // --- Drain gate: apply only on a non-empty Selection. ---
    expect(!drain_would_apply(DialogResultKind::Error, 0), "Drain_Error_NoApply");
    expect(!drain_would_apply(DialogResultKind::Cancel, 0), "Drain_Cancel_NoApply");
    expect(!drain_would_apply(DialogResultKind::Selection, 0), "Drain_EmptySelection_NoApply");
    expect(drain_would_apply(DialogResultKind::Selection, 3), "Drain_Selection_Applies");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_DialogMailbox_Unit cases passed\n";
    return 0;
}
