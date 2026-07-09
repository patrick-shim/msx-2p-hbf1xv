cmake_minimum_required(VERSION 3.21)

project(sony-msx-hbf1xv VERSION 0.1.0 LANGUAGES CXX)

option(SONY_MSX_ENABLE_SDL3 "Build SDL3 frontend" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    src/devices/cartridge/cartridge_slot.cpp
    src/devices/halnote/halnote_rom.cpp
    src/devices/kanji/kanji_font_rom.cpp
    src/devices/video/vdp_vram.cpp
    src/devices/video/v9958_vdp.cpp
    src/devices/video/sprite_engine.cpp
    src/devices/video/vdp_command_engine.cpp
    src/devices/video/vdp_frame_renderer.cpp
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
    src/machine/debug_event_log.cpp
    src/machine/frame_dump.cpp
    src/machine/input_script.cpp
    src/frontend/sdl3_cli.cpp
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

    add_library(sony_msx_sdl3_frontend STATIC
        src/frontend/sdl3_app.cpp
        src/frontend/sdl3_video_presenter.cpp
        src/frontend/sdl3_audio_presenter.cpp
        src/frontend/sdl3_input_mapper.cpp
    )
    target_link_libraries(sony_msx_sdl3_frontend PUBLIC sony_msx_core SDL3::SDL3)

    add_executable(sony_msx_sdl3 src/frontend/sdl3_main.cpp)
    target_link_libraries(sony_msx_sdl3 PRIVATE sony_msx_sdl3_frontend)
endif()

add_subdirectory(tests)
