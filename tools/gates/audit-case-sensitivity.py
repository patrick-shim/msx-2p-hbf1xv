#!/usr/bin/env python3
"""M59 S1 case-sensitivity audit (DEC-0088, docs/m59-planner-package.md section 3.4).

Verifies that the repository is safe for its FIRST case-sensitive filesystem
(Raspberry Pi ext4) by checking TRUE on-disk casing via per-component
directory-listing comparison -- so the audit is meaningful even on
case-insensitive NTFS/APFS, where a naive os.path.exists() would hide drift.

Checks (per the planner spec):
  1. Every quoted `#include "..."` in src/ (pruning src/external/) + tests/
     (if present), resolved against: the includer's own directory, src/, and
     (for the ImGui frontend includes) src/external/imgui{,/backends}.
     Case-insensitive-only resolution = FAIL; unresolvable = INFO.
  2. The fixed runtime asset-literal inventory (hardcoded below with source
     citations, planner-collected) checked the same way. Absence of an
     OPTIONAL asset (softwaredb/disks/games) = INFO; case-MISMATCH of a
     present asset = FAIL; absence of a REQUIRED asset = FAIL (availability,
     flagged as such -- not a casing defect).
  3. Generic sweep: quoted string literals matching
     [A-Za-z0-9_./-]+\\.(rom|dsk|xml|sram|wav|png) in src/ (non-external) +
     tests/; a literal that resolves on disk ONLY case-insensitively = FAIL;
     never-resolving literals = INFO (many are test-relative or synthetic).

Exit code 0 = clean (no FAIL); non-zero = FAIL findings listed.
Output mirrored to debug/logs/m59-case-audit.txt.

Stdlib-only, Python 3. Runs on Windows/macOS/Linux.
"""

from __future__ import annotations

import argparse
import os
import re
import sys

SOURCE_EXTS = {".h", ".hpp", ".hh", ".c", ".cc", ".cpp", ".inl"}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
# Quoted string literals containing an asset-looking token (planner spec 3.4 item 4).
ASSET_LITERAL_RE = re.compile(r'"([A-Za-z0-9_./-]+\.(?:rom|dsk|xml|sram|wav|png))"')

# ---------------------------------------------------------------------------
# Fixed runtime asset-literal inventory (planner package section 3.4 table (B),
# citations re-verified 2026-07-18 against the working tree).
# (literal, kind, required, citation)
#   kind: "file" | "dir"
#   required: True -> absence is FAIL (availability); False -> absence is INFO.
FIXED_INVENTORY = [
    # 7 BIOS filenames, config-fed into load_rom_assets()
    # (src/machine/emulator_config.h:48-54; also shipped sony_msx_hbf1xv.xml:138-144).
    ("bios/f1xvbios.rom", "file", True, "src/machine/emulator_config.h:48 + sony_msx_hbf1xv.xml:138"),
    ("bios/f1xvext.rom", "file", True, "src/machine/emulator_config.h:49 + sony_msx_hbf1xv.xml:139"),
    ("bios/f1xvkdr.rom", "file", True, "src/machine/emulator_config.h:50 + sony_msx_hbf1xv.xml:140"),
    ("bios/f1xvdisk.rom", "file", True, "src/machine/emulator_config.h:51 + sony_msx_hbf1xv.xml:141"),
    ("bios/f1xvmus.rom", "file", True, "src/machine/emulator_config.h:52 + sony_msx_hbf1xv.xml:142"),
    ("bios/f1xvkfn.rom", "file", True, "src/machine/emulator_config.h:53 + sony_msx_hbf1xv.xml:143"),
    ("bios/f1xvfirm.rom", "file", True, "src/machine/emulator_config.h:54 + sony_msx_hbf1xv.xml:144"),
    # BIOS dir literal "bios" (4 source sites + shipped XML dir attribute).
    ("bios", "dir", True,
     "src/frontend/config_runtime.h:113 / src/machine/emulator_config.h:87 / "
     "src/machine/hbf1xv_machine.h:1054 / src/frontend/sdl3_app.h:38 / sony_msx_hbf1xv.xml:137"),
    # FM-PAC peripheral pair (src/machine/emulator_config.h:89-90; sony_msx_hbf1xv.xml:149).
    ("roms/fmpac.rom", "file", True, "src/machine/emulator_config.h:89 + sony_msx_hbf1xv.xml:149"),
    ("roms/fmpac.rom.sram", "file", True, "src/machine/emulator_config.h:90 + sony_msx_hbf1xv.xml:149"),
    # Root reference config, auto-searched exe-dir then CWD (src/frontend/sdl3_main.cpp:399-401).
    ("sony_msx_hbf1xv.xml", "file", True, "src/frontend/sdl3_main.cpp:399-401"),
    # softwaredb default -- OPTIONAL: absent on fresh clones, graceful fallback
    # (src/machine/cartridge_identifier.h:39; cartridge_identifier.cpp:316-322).
    ("references/openmsx-21.0/share/softwaredb.xml", "file", False,
     "src/machine/cartridge_identifier.h:39 + sony_msx_hbf1xv.xml:153"),
]


class ListingCache:
    """Per-directory os.listdir() cache for true-casing comparisons."""

    def __init__(self) -> None:
        self._cache: dict[str, list[str] | None] = {}

    def listing(self, directory: str) -> list[str] | None:
        key = os.path.normpath(directory)
        if key not in self._cache:
            try:
                self._cache[key] = os.listdir(key)
            except OSError:
                self._cache[key] = None
        return self._cache[key]


def true_case_resolve(root: str, rel_path: str, cache: ListingCache):
    """Resolve rel_path under root using case-INSENSITIVE per-component matching,
    comparing every component against the actual directory listing.

    Returns (status, detail):
      status "ok"        -- resolves with exact casing at every component
      status "mismatch"  -- resolves only case-insensitively; detail = actual on-disk relpath
      status "absent"    -- some component does not exist even case-insensitively
    """
    parts = [p for p in os.path.normpath(rel_path).replace("\\", "/").split("/")
             if p not in ("", ".")]
    current = root
    actual_parts: list[str] = []
    mismatch = False
    for part in parts:
        if part == "..":
            current = os.path.dirname(current) or current
            if actual_parts:
                actual_parts.pop()
            continue
        entries = cache.listing(current)
        if entries is None:
            return "absent", None
        if part in entries:  # exact-case hit
            actual_parts.append(part)
            current = os.path.join(current, part)
            continue
        lowered = part.lower()
        ci_hits = [e for e in entries if e.lower() == lowered]
        if not ci_hits:
            return "absent", None
        mismatch = True
        actual_parts.append(ci_hits[0])
        current = os.path.join(current, ci_hits[0])
    if mismatch:
        return "mismatch", "/".join(actual_parts)
    return "ok", "/".join(actual_parts)


def iter_source_files(root: str):
    """Yield project source files: src/ (pruning src/external/) + tests/ if present."""
    src_root = os.path.join(root, "src")
    external = os.path.normpath(os.path.join(src_root, "external"))
    for base in (src_root, os.path.join(root, "tests")):
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            if os.path.normpath(dirpath) == external:
                dirnames[:] = []
                continue
            dirnames[:] = [d for d in dirnames
                           if os.path.normpath(os.path.join(dirpath, d)) != external]
            for name in filenames:
                if os.path.splitext(name)[1].lower() in SOURCE_EXTS:
                    yield os.path.join(dirpath, name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=None,
                        help="repository root (default: parent of this script's directory)")
    args = parser.parse_args()

    root = os.path.abspath(args.root) if args.root else \
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cache = ListingCache()
    lines: list[str] = []
    fails: list[str] = []
    infos: list[str] = []

    def emit(text: str = "") -> None:
        lines.append(text)

    emit("M59 case-sensitivity audit (tools/gates/audit-case-sensitivity.py)")
    emit(f"root: {root}")
    emit()

    # ---- Check 1: #include "..." true-casing ------------------------------
    include_roots_common = [os.path.join(root, "src")]
    imgui_roots = [os.path.join(root, "src", "external", "imgui"),
                   os.path.join(root, "src", "external", "imgui", "backends")]
    files_scanned = 0
    includes_checked = 0
    for path in sorted(iter_source_files(root)):
        files_scanned += 1
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError as exc:
            infos.append(f"INFO include-scan: cannot read {path}: {exc}")
            continue
        rel_file = os.path.relpath(path, root).replace("\\", "/")
        for lineno, line in enumerate(text.splitlines(), 1):
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(1)
            includes_checked += 1
            candidates = [os.path.dirname(path)] + include_roots_common + imgui_roots
            status_final = "absent"
            for cand in candidates:
                status, detail = true_case_resolve(cand, inc, cache)
                if status == "ok":
                    status_final = "ok"
                    break
                if status == "mismatch":
                    status_final = "mismatch"
                    fails.append(
                        f'FAIL include-casing: {rel_file}:{lineno} #include "{inc}" '
                        f"resolves only case-insensitively (on disk: {detail} under "
                        f"{os.path.relpath(cand, root)})")
                    break
            if status_final == "absent":
                infos.append(
                    f'INFO include-unresolved: {rel_file}:{lineno} #include "{inc}" '
                    f"(not found under includer dir / src / imgui dirs -- generated or external?)")
    emit(f"[1] include audit: {files_scanned} source files, {includes_checked} quoted includes checked")

    # ---- Check 2: fixed asset-literal inventory ---------------------------
    emit(f"[2] fixed asset-literal inventory: {len(FIXED_INVENTORY)} entries")
    for literal, kind, required, citation in FIXED_INVENTORY:
        status, detail = true_case_resolve(root, literal, cache)
        if status == "ok":
            is_dir = os.path.isdir(os.path.join(root, *literal.split("/")))
            if kind == "dir" and not is_dir:
                fails.append(f"FAIL inventory: {literal} exists but is not a directory ({citation})")
            else:
                emit(f"    OK   {literal}  ({citation})")
        elif status == "mismatch":
            fails.append(f"FAIL inventory-casing: {literal} on disk as {detail}  ({citation})")
        else:  # absent
            if required:
                fails.append(f"FAIL inventory-absent (availability, not casing): {literal}  ({citation})")
            else:
                infos.append(f"INFO inventory-optional-absent: {literal}  ({citation})")

    # ---- Check 3: generic asset-literal sweep -----------------------------
    generic: dict[str, list[str]] = {}
    for path in sorted(iter_source_files(root)):
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        rel_file = os.path.relpath(path, root).replace("\\", "/")
        for lineno, line in enumerate(text.splitlines(), 1):
            for m in ASSET_LITERAL_RE.finditer(line):
                generic.setdefault(m.group(1), []).append(f"{rel_file}:{lineno}")
    emit(f"[3] generic literal sweep: {len(generic)} distinct asset-looking literals")
    fixed_set = {lit for lit, _, _, _ in FIXED_INVENTORY}
    resolved_ok = 0
    for literal in sorted(generic):
        occurrences = generic[literal]
        # Resolution candidates: repo root (runtime CWD convention); bare
        # filenames additionally under bios/ (the config-fed BIOS names).
        candidates = [root]
        if "/" not in literal:
            candidates.append(os.path.join(root, "bios"))
        status_final = "absent"
        for cand in candidates:
            status, detail = true_case_resolve(cand, literal, cache)
            if status == "ok":
                status_final = "ok"
                break
            if status == "mismatch":
                status_final = "mismatch"
                fails.append(
                    f"FAIL literal-casing: \"{literal}\" on disk as {detail} "
                    f"(sites: {', '.join(occurrences[:4])}{'...' if len(occurrences) > 4 else ''})")
                break
        if status_final == "ok":
            resolved_ok += 1
        elif status_final == "absent" and literal not in fixed_set:
            infos.append(
                f"INFO literal-unresolved: \"{literal}\" never resolves from repo root "
                f"({len(occurrences)} site(s), e.g. {occurrences[0]}) -- test-relative or synthetic")
    emit(f"    {resolved_ok} resolve with exact casing; "
         f"{sum(1 for f in fails if f.startswith('FAIL literal'))} casing FAIL(s); rest INFO")

    # ---- Report ------------------------------------------------------------
    emit()
    if fails:
        emit(f"RESULT: FAIL ({len(fails)} finding(s))")
        for f in fails:
            emit("  " + f)
    else:
        emit("RESULT: CLEAN (0 FAIL findings)")
    if infos:
        emit(f"INFO items ({len(infos)}):")
        for i in infos:
            emit("  " + i)

    report = "\n".join(lines) + "\n"
    sys.stdout.write(report)

    log_dir = os.path.join(root, "debug", "logs")
    try:
        os.makedirs(log_dir, exist_ok=True)
        with open(os.path.join(log_dir, "m59-case-audit.txt"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(report)
    except OSError as exc:
        sys.stderr.write(f"warning: could not write debug/logs/m59-case-audit.txt: {exc}\n")

    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
