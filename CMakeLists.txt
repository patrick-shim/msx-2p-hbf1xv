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

# MSVC otherwise reads sources in the system code page (cp949 on the author's
# machine) and warns C4819 on files carrying UTF-8 comments (em-dashes, etc.).
# /utf-8 makes MSVC treat sources as UTF-8. Placed before any target so the core
# library, both executables, and the tests/ subdirectory all inherit it; no-op
# on GCC/Clang.
if(MSVC)
    add_compile_options(/utf-8)
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
    src/machine/software_db.cpp
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
    src/frontend/session_summary.cpp
    src/frontend/border_composer.cpp
    src/frontend/audio_pacer.cpp
    src/frontend/psg_audio_pump.cpp
    src/frontend/machine_audio_mixer.cpp
    src/frontend/psg_audio_dump.cpp
)

target_include_directories(sony_msx_core PUBLIC src)

add_executable(sony_msx_headless src/main.cpp)
target_link_libraries(sony_msx_headless PRIVATE sony_msx_core)

if(SONY_MSX_ENABLE_SDL3)
    find_package(SDL3 CONFIG REQUIRED)

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
    )
    target_link_libraries(sony_msx_sdl3_frontend PUBLIC sony_msx_core SDL3::SDL3)

    add_executable(sony_msx_sdl3 src/frontend/sdl3_main.cpp)
    target_link_libraries(sony_msx_sdl3 PRIVATE sony_msx_sdl3_frontend)
    sony_msx_stage_sdl3_dll(sony_msx_sdl3)
endif()

# Guarded for lean remote checkouts (DEC-0044): a clone without tests/ (the
# owner's compile-only remote scope) must still configure and build both
# executables; the full test suite exists in the primary repository.
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
