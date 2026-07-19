# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator
#  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
#
#  LEGAL NOTICE - Personal reference only.
#  This source code is made available solely for personal, non-commercial
#  reference and educational study. Commercial use, sale, or redistribution
#  for profit is not permitted without the author's written consent.
#  Provided "AS IS", without warranty of any kind.
#  Proprietary BIOS/ROM/disk assets remain the property of their respective
#  rights holders and are NOT licensed by this notice.
# ============================================================================

cmake_minimum_required(VERSION 3.21)

project(sony-msx-hbf1xv VERSION 0.1.0 LANGUAGES CXX)

option(SONY_MSX_ENABLE_SDL3 "Build SDL3 frontend" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform warning/encoding matrix (M54/DEC-0081). ONE codebase, two toolchains,
# auto-selected by the target system -- no fork, no per-platform build file.
# Placed before any target so the core library, every executable, and the tests/
# subdirectory all inherit the flags.
#
# MSVC: reads sources in the system code page (cp949 on the author's machine) and
# warns C4819 on files carrying UTF-8 comments (em-dashes, etc.). /utf-8 makes
# MSVC treat sources as UTF-8. Clang/GCC are UTF-8 native and need no equivalent.
#
# AppleClang/Clang/GCC: -Wall -Wextra mirrors the MSVC intent (diagnostics on)
# without changing what builds. -Werror is deliberately NOT set: MSVC carries no
# /WX counterpart, so adding it would make the toolchains asymmetric and let a
# benign warning block the build. else() rather than if(APPLE) so a future
# Linux/GCC host inherits the same regime.
if(MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

enable_testing()

add_library(sony_msx_core STATIC
    src/devices/cpu/cpu_bus_client.cpp
    src/devices/cpu/z80a_state.cpp
    src/devices/cpu/z80a_cpu.cpp
    src/devices/chipset/slot_bus.cpp
    src/devices/chipset/ppi_slot_select.cpp
    src/devices/chipset/ppi_8255.cpp
    src/devices/chipset/io_bus.cpp
    src/devices/chipset/mapper_io.cpp
    src/devices/chipset/switched_io.cpp
    src/devices/chipset/s1985_engine.cpp
    src/devices/chipset/reset_status_register.cpp
    src/devices/chipset/system_bus.cpp
    src/devices/chipset/system_control.cpp
    src/devices/chipset/mb670836_pause.cpp
    src/devices/audio/psg_ym2149.cpp
    src/devices/audio/click_dac.cpp
    src/devices/audio/scc_wavetable.cpp
    src/devices/audio/ym2413_opll.cpp
    src/devices/audio/ym2413_synth.cpp
    src/devices/rtc/rp5c01.cpp
    src/devices/fdc/disk_image.cpp
    src/devices/fdc/disk_drive.cpp
    src/devices/fdc/wd2793.cpp
    src/devices/fdc/sony_fdc.cpp
    src/devices/memory/rom_device.cpp
    src/devices/memory/memory_mapper_ram.cpp
    src/devices/memory/battery_backed_sram.cpp
    src/devices/cartridge/cartridge_mapper_type.cpp
    src/devices/cartridge/cartridge_rom_window.cpp
    src/devices/cartridge/cartridge_mirrored_rom.cpp
    src/devices/cartridge/cartridge_generic8kb_rom.cpp
    src/devices/cartridge/cartridge_generic16kb_rom.cpp
    src/devices/cartridge/cartridge_ascii8kb_rom.cpp
    src/devices/cartridge/cartridge_ascii16kb_rom.cpp
    src/devices/cartridge/cartridge_konami_rom.cpp
    src/devices/cartridge/cartridge_konami_scc_rom.cpp
    src/devices/cartridge/cartridge_fmpac_rom.cpp
    src/devices/cartridge/cartridge_slot.cpp
    src/devices/halnote/halnote_rom.cpp
    src/devices/kanji/kanji_font_rom.cpp
    src/devices/video/vdp_vram.cpp
    src/devices/video/v9958_vdp.cpp
    src/devices/video/sprite_engine.cpp
    src/devices/video/vdp_command_engine.cpp
    src/devices/video/vdp_frame_renderer.cpp
    src/devices/video/vdp_scanline_accumulator.cpp
    src/peripherals/keyboard_matrix.cpp
    src/peripherals/joystick.cpp
    src/peripherals/printer_port.cpp
    src/peripherals/cassette_interface.cpp
    src/peripherals/rensha_turbo.cpp
    src/peripherals/msx_key_names.cpp
    src/core/scheduler.cpp
    src/machine/hbf1xv_machine.cpp
    src/machine/rom_asset_loader.cpp
    src/machine/cartridge_cli.cpp
    src/machine/sha1.cpp
    src/machine/xml_tokenizer.cpp
    src/machine/software_db.cpp
    src/machine/emulator_config.cpp
    src/machine/cartridge_identifier.cpp
    src/machine/cpu_trace_sink.cpp
    src/machine/cpm_bdos_harness.cpp
    src/machine/memory_region.cpp
    src/machine/debug_dump.cpp
    src/machine/debug_snapshot.cpp
    src/machine/debug_event_log.cpp
    src/machine/frame_dump.cpp
    src/machine/input_script.cpp
    src/frontend/sdl3_cli.cpp
    src/frontend/config_runtime.cpp
    src/frontend/config_xml_writer.cpp
    src/frontend/recent_files.cpp
    src/frontend/session_summary.cpp
    src/frontend/menu_model.cpp
    src/frontend/blank_disk_image.cpp
    src/frontend/border_composer.cpp
    src/frontend/audio_pacer.cpp
    src/frontend/psg_audio_pump.cpp
    src/frontend/machine_audio_mixer.cpp
    src/frontend/psg_audio_dump.cpp
)

target_include_directories(sony_msx_core PUBLIC src)

add_executable(sony_msx_headless src/main.cpp)
target_link_libraries(sony_msx_headless PRIVATE sony_msx_core)

# --- M53 (DEC-0080): standalone host-side MSX disk utility --------------------
# Build-ISOLATED: msx_diskutil links NOTHING from sony_msx_core and sony_msx_core
# never links it (one-way isolation). Additive only; zero effect on the emulator
# targets or the deterministic suite. Placed OUTSIDE the SDL3 guard so it always
# builds in the standard flow.
add_library(msx_diskutil STATIC
    src/utils/msx_disk_format.cpp
    src/utils/hex_dump.cpp
    src/utils/cli.cpp
)
target_include_directories(msx_diskutil PUBLIC src)

add_executable(msx_disk src/utils/main.cpp)
target_link_libraries(msx_disk PRIVATE msx_diskutil)
set_target_properties(msx_disk PROPERTIES OUTPUT_NAME msx-diskutil)

# Post-build: copy the ONE build-tree binary into the owner-created root
# utils/ (DEC-0041: no second build tree). make_directory guards a fresh
# clone / lean checkout where the folder is absent (R7). $<TARGET_FILE:msx_disk>
# resolves the platform suffix (msx-diskutil.exe on Windows, msx-diskutil elsewhere).
add_custom_command(TARGET msx_disk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/utils"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:msx_disk>" "${CMAKE_SOURCE_DIR}/utils/"
    VERBATIM)

if(SONY_MSX_ENABLE_SDL3)
    find_package(SDL3 CONFIG REQUIRED)

    # --- M55 (DEC-0083): Dear ImGui in-window menu-bar dependency --------------
    # ImGui is MIT-licensed (src/external/imgui/LICENSE.txt) so, unlike the GPL
    # openMSX / non-commercial fMSX reference trees, its vendored sources are
    # COMPILED into our binary -- the owner-ratified "vendored like SDL3" model.
    # Owner-vendored tree: 1.92.9 WIP (IMGUI_VERSION_NUM 19285).
    #
    # EXACTLY 6 translation units: the 4 core .cpp + the 2 official SDL3 backends
    # (imgui_impl_sdl3 platform backend + imgui_impl_sdlrenderer3 renderer
    # backend). imgui_demo.cpp is DELIBERATELY NOT compiled (DEC-0083 R5 "no
    # extras"); no docking/freetype/other backend is a build input. This target
    # is only ever evaluated when SONY_MSX_ENABLE_SDL3=ON and is linked ONLY into
    # sony_msx_sdl3_frontend below -- the headless / msx_disk / core targets never
    # see it, so the default (=OFF) headless configuration compiles ZERO ImGui TUs.
    add_library(imgui STATIC
        src/external/imgui/imgui.cpp
        src/external/imgui/imgui_draw.cpp
        src/external/imgui/imgui_tables.cpp
        src/external/imgui/imgui_widgets.cpp
        src/external/imgui/backends/imgui_impl_sdl3.cpp
        src/external/imgui/backends/imgui_impl_sdlrenderer3.cpp)
    target_include_directories(imgui PUBLIC
        src/external/imgui
        src/external/imgui/backends)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
    # A5: third-party sources are not held to the project's -Wall -Wextra intent;
    # target-scoped warning suppression keeps ImGui's noise out of our build log
    # (the repo sets no -Werror, so this is cosmetic hygiene, not a build gate).
    if(MSVC)
        target_compile_options(imgui PRIVATE /w)
    else()
        target_compile_options(imgui PRIVATE -w)
    endif()

    # SDL3 is a SHARED library on Windows: every executable that links it
    # (directly or transitively via sony_msx_sdl3_frontend) needs SDL3.dll
    # beside it, or the Windows loader aborts with 0xc0000135 AND a MODAL
    # error dialog that hangs unattended ctest runs (M33/DEC-0041 finding).
    # Staging the DLL at build time per target makes every build path --
    # tools/bootstrap-build.ps1, bare cmake, or an IDE -- self-sufficient.
    if(TARGET SDL3::SDL3-shared)
        set(SONY_MSX_SDL3_RUNTIME_TARGET SDL3::SDL3-shared)
    else()
        set(SONY_MSX_SDL3_RUNTIME_TARGET SDL3::SDL3)
    endif()
    function(sony_msx_stage_sdl3_dll target)
        if(WIN32)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_FILE:${SONY_MSX_SDL3_RUNTIME_TARGET}>
                        $<TARGET_FILE_DIR:${target}>
                VERBATIM)
        endif()
    endfunction()

    add_library(sony_msx_sdl3_frontend STATIC
        src/frontend/sdl3_app.cpp
        src/frontend/sdl3_video_presenter.cpp
        src/frontend/sdl3_audio_presenter.cpp
        src/frontend/sdl3_input_mapper.cpp
        src/frontend/sdl3_menu.cpp          # M55 (DEC-0083): ImGui menu view/controller
    )
    # M55 (DEC-0083): + imgui. The menu-model TU (SDL/ImGui-free) is compiled into
    # sony_msx_core (see the core source list) so its unit test registers OUTSIDE
    # the SDL3 guard, the established SDL-free-frontend-logic pattern.
    target_link_libraries(sony_msx_sdl3_frontend PUBLIC sony_msx_core SDL3::SDL3 imgui)

    add_executable(sony_msx_sdl3 src/frontend/sdl3_main.cpp)
    target_link_libraries(sony_msx_sdl3 PRIVATE sony_msx_sdl3_frontend)
    sony_msx_stage_sdl3_dll(sony_msx_sdl3)

    # Windows-only: give sony_msx_sdl3.exe its Explorer/taskbar icon via a
    # compiled resource. configure_file() bakes an absolute, forward-slash path
    # to the tracked assets/app_icon.ico into ${build}/app_icon.rc so the .rc
    # resolves from any build tree or fresh clone (RC does not search the .rc's
    # own directory). MSVC (and MinGW windres) compile the .rc automatically.
    # Runtime window/taskbar icon is additionally set via SDL_SetWindowIcon().
    if(WIN32)
        set(SONY_MSX_APP_ICON_ICO "${CMAKE_SOURCE_DIR}/assets/icons/msx_app.ico")
        configure_file(
            "${CMAKE_SOURCE_DIR}/src/frontend/app_icon.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc"
            @ONLY)
        target_sources(sony_msx_sdl3 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    endif()
endif()

# Guarded for lean remote checkouts (DEC-0044): a clone without tests/ (the
# owner's compile-only remote scope) must still configure and build both
# executables; the full test suite exists in the primary repository.
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
