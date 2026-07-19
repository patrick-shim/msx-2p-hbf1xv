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

#pragma once

#include <string>

#include "machine/emulator_config.h"

// The INVERSE of EmulatorConfig::parse: serialize an EmulatorConfig back to the
// strict `<hbf1xv-config>` XML the machine-layer parser reads. This is the
// first-ever emitter for that schema (the parser shipped without one -- see
// src/machine/emulator_config.cpp). It exists so the SDL3 frontend can PERSIST
// interactive settings to `sony_msx_hbf1xv.xml` at the PROJECT ROOT (the same
// file the auto-load search consults). There is no longer a shipped reference
// config in the repository -- the emulator writes the file itself on first
// change, so this emitter IS how a user's config comes into existence.
//
// DELIBERATELY SDL-free + pure, and DELIBERATELY here (frontend) rather than as
// an EmulatorConfig member: the persistence feature is a frontend concern and
// the machine layer stays untouched. The cost is that this emitter and
// EmulatorConfig::parse must be kept schema-synced by hand; the round-trip
// invariant below is the guard that catches drift.
//
// ROUND-TRIP INVARIANT (tested by frontend_config_xml_writer_unit_test): for
// every EmulatorConfig `cfg` reachable by parse(), `parse(emulator_config_to_xml(
// cfg), warnings)` yields a config EQUAL to `cfg` with an EMPTY `warnings` vector.
// Emitting the built-in defaults (`EmulatorConfig{}`) therefore produces a
// document that parses to the compiled defaults with zero warnings and documents
// every knob -- the in-memory anti-drift guard (the old file-based repo-root
// shipped-config round-trip test was retired in favor of this one; the only
// runtime config lives beside the exe -- DEC-0095-AMENDMENT-C).
namespace sony_msx::frontend {

// Serialize `cfg` to a complete, human-readable, round-trippable
// `<hbf1xv-config>` XML document (newline-terminated). String/path attribute
// values are XML-escaped; the whole `<defaults>` + `<machine>` schema is
// emitted so nothing a user placed in the file is silently dropped on rewrite.
[[nodiscard]] std::string emulator_config_to_xml(const machine::EmulatorConfig& cfg);

}  // namespace sony_msx::frontend
