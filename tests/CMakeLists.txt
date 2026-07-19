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

add_executable(core_scheduler_unit_test
    unit/core/scheduler_unit_test.cpp
)
target_link_libraries(core_scheduler_unit_test PRIVATE sony_msx_core)
add_test(NAME core_scheduler_unit_test COMMAND core_scheduler_unit_test)

add_executable(core_bus_contract_unit_test
    unit/core/bus_contract_unit_test.cpp
)
target_link_libraries(core_bus_contract_unit_test PRIVATE sony_msx_core)
add_test(NAME core_bus_contract_unit_test COMMAND core_bus_contract_unit_test)

add_executable(devices_cpu_bus_client_unit_test
    unit/devices/cpu_bus_client_unit_test.cpp
)
target_link_libraries(devices_cpu_bus_client_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cpu_bus_client_unit_test COMMAND devices_cpu_bus_client_unit_test)

add_executable(devices_cpu_io_seam_unit_test
    unit/devices/cpu_io_seam_unit_test.cpp
)
target_link_libraries(devices_cpu_io_seam_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cpu_io_seam_unit_test COMMAND devices_cpu_io_seam_unit_test)

add_executable(devices_chipset_slot_bus_unit_test
    unit/devices/chipset_slot_bus_unit_test.cpp
)
target_link_libraries(devices_chipset_slot_bus_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_slot_bus_unit_test COMMAND devices_chipset_slot_bus_unit_test)

add_executable(devices_chipset_ppi_slot_select_unit_test
    unit/devices/chipset_ppi_slot_select_unit_test.cpp
)
target_link_libraries(devices_chipset_ppi_slot_select_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_ppi_slot_select_unit_test COMMAND devices_chipset_ppi_slot_select_unit_test)

add_executable(devices_chipset_io_bus_unit_test
    unit/devices/chipset_io_bus_unit_test.cpp
)
target_link_libraries(devices_chipset_io_bus_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_io_bus_unit_test COMMAND devices_chipset_io_bus_unit_test)

add_executable(devices_chipset_backup_ram_unit_test
    unit/devices/chipset_backup_ram_unit_test.cpp
)
target_link_libraries(devices_chipset_backup_ram_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_backup_ram_unit_test COMMAND devices_chipset_backup_ram_unit_test)

add_executable(devices_chipset_mapper_io_unit_test
    unit/devices/chipset_mapper_io_unit_test.cpp
)
target_link_libraries(devices_chipset_mapper_io_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_mapper_io_unit_test COMMAND devices_chipset_mapper_io_unit_test)

# --- M13 memory devices + asset loader --------------------------------------
# Absolute path to the repo bios/ directory so tests that need real ROM images
# (asset loader, slot map, boot checkpoint, mem parity) do not depend on cwd.
set(SONY_MSX_BIOS_DIR_ABS "${CMAKE_SOURCE_DIR}/bios")

add_executable(devices_memory_rom_device_unit_test
    unit/devices/memory_rom_device_unit_test.cpp
)
target_link_libraries(devices_memory_rom_device_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_memory_rom_device_unit_test COMMAND devices_memory_rom_device_unit_test)

add_executable(devices_memory_mapper_ram_unit_test
    unit/devices/memory_mapper_ram_unit_test.cpp
)
target_link_libraries(devices_memory_mapper_ram_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_memory_mapper_ram_unit_test COMMAND devices_memory_mapper_ram_unit_test)

add_executable(machine_rom_asset_loader_unit_test
    unit/machine/rom_asset_loader_unit_test.cpp
)
target_link_libraries(machine_rom_asset_loader_unit_test PRIVATE sony_msx_core)
target_compile_definitions(machine_rom_asset_loader_unit_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_rom_asset_loader_unit_test COMMAND machine_rom_asset_loader_unit_test)

add_executable(machine_hbf1xv_bios_boot_integration_test
    integration/machine/hbf1xv_bios_boot_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_bios_boot_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_bios_boot_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_bios_boot_integration_test COMMAND machine_hbf1xv_bios_boot_integration_test)

add_executable(machine_hbf1xv_mem_parity_checkpoint_integration_test
    integration/machine/hbf1xv_mem_parity_checkpoint_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_mem_parity_checkpoint_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_mem_parity_checkpoint_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_mem_parity_checkpoint_integration_test
    COMMAND machine_hbf1xv_mem_parity_checkpoint_integration_test)

add_executable(devices_chipset_m1_wait_unit_test
    unit/devices/chipset_m1_wait_unit_test.cpp
)
target_link_libraries(devices_chipset_m1_wait_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_m1_wait_unit_test COMMAND devices_chipset_m1_wait_unit_test)

add_executable(devices_z80a_state_unit_test
    unit/devices/z80a_state_unit_test.cpp
)
target_link_libraries(devices_z80a_state_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_state_unit_test COMMAND devices_z80a_state_unit_test)

add_executable(devices_z80a_unprefixed_unit_test
    unit/devices/z80a_unprefixed_unit_test.cpp
)
target_link_libraries(devices_z80a_unprefixed_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_unprefixed_unit_test COMMAND devices_z80a_unprefixed_unit_test)

add_executable(devices_z80a_interrupts_unit_test
    unit/devices/z80a_interrupts_unit_test.cpp
)
target_link_libraries(devices_z80a_interrupts_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_interrupts_unit_test COMMAND devices_z80a_interrupts_unit_test)

add_executable(devices_z80a_unprefixed_ops_unit_test
    unit/devices/z80a_unprefixed_ops_unit_test.cpp
)
target_link_libraries(devices_z80a_unprefixed_ops_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_unprefixed_ops_unit_test COMMAND devices_z80a_unprefixed_ops_unit_test)

add_executable(devices_z80a_cb_prefixed_unit_test
    unit/devices/z80a_cb_prefixed_unit_test.cpp
)
target_link_libraries(devices_z80a_cb_prefixed_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_cb_prefixed_unit_test COMMAND devices_z80a_cb_prefixed_unit_test)

add_executable(devices_z80a_ed_prefixed_unit_test
    unit/devices/z80a_ed_prefixed_unit_test.cpp
)
target_link_libraries(devices_z80a_ed_prefixed_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_ed_prefixed_unit_test COMMAND devices_z80a_ed_prefixed_unit_test)

add_executable(devices_z80a_indexed_unit_test
    unit/devices/z80a_indexed_unit_test.cpp
)
target_link_libraries(devices_z80a_indexed_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_indexed_unit_test COMMAND devices_z80a_indexed_unit_test)

add_executable(devices_z80a_indexed_cb_unit_test
    unit/devices/z80a_indexed_cb_unit_test.cpp
)
target_link_libraries(devices_z80a_indexed_cb_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_indexed_cb_unit_test COMMAND devices_z80a_indexed_cb_unit_test)

add_executable(devices_z80a_interrupt_modes_unit_test
    unit/devices/z80a_interrupt_modes_unit_test.cpp
)
target_link_libraries(devices_z80a_interrupt_modes_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_interrupt_modes_unit_test COMMAND devices_z80a_interrupt_modes_unit_test)

add_executable(devices_z80a_trace_export_unit_test
    unit/devices/z80a_trace_export_unit_test.cpp
)
target_link_libraries(devices_z80a_trace_export_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_trace_export_unit_test COMMAND devices_z80a_trace_export_unit_test)

add_executable(devices_z80a_wz_memptr_unit_test
    unit/devices/z80a_wz_memptr_unit_test.cpp
)
target_link_libraries(devices_z80a_wz_memptr_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_wz_memptr_unit_test COMMAND devices_z80a_wz_memptr_unit_test)

add_executable(devices_z80a_scf_ccf_q_unit_test
    unit/devices/z80a_scf_ccf_q_unit_test.cpp
)
target_link_libraries(devices_z80a_scf_ccf_q_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_scf_ccf_q_unit_test COMMAND devices_z80a_scf_ccf_q_unit_test)

add_executable(devices_z80a_nmos_interrupt_unit_test
    unit/devices/z80a_nmos_interrupt_unit_test.cpp
)
target_link_libraries(devices_z80a_nmos_interrupt_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_nmos_interrupt_unit_test COMMAND devices_z80a_nmos_interrupt_unit_test)

add_executable(devices_z80a_parity_undocumented_unit_test
    unit/devices/z80a_parity_undocumented_unit_test.cpp
)
target_link_libraries(devices_z80a_parity_undocumented_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_parity_undocumented_unit_test COMMAND devices_z80a_parity_undocumented_unit_test)

add_executable(devices_z80a_block_flags_unit_test
    unit/devices/z80a_block_flags_unit_test.cpp
)
target_link_libraries(devices_z80a_block_flags_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_block_flags_unit_test COMMAND devices_z80a_block_flags_unit_test)

add_executable(devices_z80a_16bit_daa_unit_test
    unit/devices/z80a_16bit_daa_unit_test.cpp
)
target_link_libraries(devices_z80a_16bit_daa_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_16bit_daa_unit_test COMMAND devices_z80a_16bit_daa_unit_test)

add_executable(machine_hbf1xv_memory_regions_unit_test
    unit/machine/hbf1xv_memory_regions_unit_test.cpp
)
target_link_libraries(machine_hbf1xv_memory_regions_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_memory_regions_unit_test COMMAND machine_hbf1xv_memory_regions_unit_test)

add_executable(machine_hbf1xv_slot_map_unit_test
    unit/machine/hbf1xv_slot_map_unit_test.cpp
)
target_link_libraries(machine_hbf1xv_slot_map_unit_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_slot_map_unit_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_slot_map_unit_test COMMAND machine_hbf1xv_slot_map_unit_test)

add_executable(machine_hbf1xv_debug_dump_unit_test
    unit/machine/hbf1xv_debug_dump_unit_test.cpp
)
target_link_libraries(machine_hbf1xv_debug_dump_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_debug_dump_unit_test COMMAND machine_hbf1xv_debug_dump_unit_test)

add_executable(machine_hbf1xv_machine_integration_test
    integration/machine/hbf1xv_machine_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_machine_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_machine_integration_test COMMAND machine_hbf1xv_machine_integration_test)

add_executable(machine_hbf1xv_debug_event_log_integration_test
    integration/machine/hbf1xv_debug_event_log_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_debug_event_log_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_debug_event_log_integration_test COMMAND machine_hbf1xv_debug_event_log_integration_test)

add_executable(machine_hbf1xv_memory_regions_integration_test
    integration/machine/hbf1xv_memory_regions_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_memory_regions_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_memory_regions_integration_test COMMAND machine_hbf1xv_memory_regions_integration_test)

# M42 (DEC-0061): opt-in --ram <64|128|256|512> main-RAM sizing across the full
# machine bus fabric (mapper_io #FC -> MemoryMapperRam fold -> dram_).
add_executable(machine_hbf1xv_ram_size_integration_test
    integration/machine/hbf1xv_ram_size_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_ram_size_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_ram_size_integration_test COMMAND machine_hbf1xv_ram_size_integration_test)

# M42 QA (DEC-0061): end-to-end "software SEES the extra RAM" proof, matched to
# the openMSX A/B -- real BIOS cold-boot + live Z80 run, then the 32-segment
# mapper-detection probe (32 distinct at 512 KB, 4 at stock 64 KB). Needs the
# repo bios/ dir (real ROMs) via SONY_MSX_BIOS_DIR.
add_executable(machine_hbf1xv_ram_detect_boot_integration_test
    integration/machine/hbf1xv_ram_detect_boot_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_ram_detect_boot_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_ram_detect_boot_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_ram_detect_boot_integration_test COMMAND machine_hbf1xv_ram_detect_boot_integration_test)

add_executable(machine_hbf1xv_system_bus_integration_test
    integration/machine/hbf1xv_system_bus_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_system_bus_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_system_bus_integration_test COMMAND machine_hbf1xv_system_bus_integration_test)

add_executable(machine_hbf1xv_cpu_step_integration_test
    integration/machine/hbf1xv_cpu_step_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cpu_step_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_cpu_step_integration_test COMMAND machine_hbf1xv_cpu_step_integration_test)

add_executable(machine_hbf1xv_trace_program_integration_test
    integration/machine/hbf1xv_trace_program_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_trace_program_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_trace_program_integration_test COMMAND machine_hbf1xv_trace_program_integration_test)

add_executable(machine_hbf1xv_cb_program_integration_test
    integration/machine/hbf1xv_cb_program_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cb_program_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_cb_program_integration_test COMMAND machine_hbf1xv_cb_program_integration_test)

add_executable(machine_hbf1xv_ldir_program_integration_test
    integration/machine/hbf1xv_ldir_program_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_ldir_program_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_ldir_program_integration_test COMMAND machine_hbf1xv_ldir_program_integration_test)

add_executable(machine_hbf1xv_indexed_program_integration_test
    integration/machine/hbf1xv_indexed_program_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_indexed_program_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_indexed_program_integration_test COMMAND machine_hbf1xv_indexed_program_integration_test)

add_executable(machine_hbf1xv_interrupt_ack_integration_test
    integration/machine/hbf1xv_interrupt_ack_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_interrupt_ack_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_interrupt_ack_integration_test COMMAND machine_hbf1xv_interrupt_ack_integration_test)

add_executable(machine_hbf1xv_cpu_parity_integration_test
    integration/machine/hbf1xv_cpu_parity_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cpu_parity_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_cpu_parity_integration_test COMMAND machine_hbf1xv_cpu_parity_integration_test)

add_executable(machine_hbf1xv_io_parity_checkpoint_integration_test
    integration/machine/hbf1xv_io_parity_checkpoint_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_io_parity_checkpoint_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_io_parity_checkpoint_integration_test COMMAND machine_hbf1xv_io_parity_checkpoint_integration_test)

add_executable(machine_hbf1xv_parity_checkpoint_integration_test
    integration/machine/hbf1xv_parity_checkpoint_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_parity_checkpoint_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_parity_checkpoint_integration_test COMMAND machine_hbf1xv_parity_checkpoint_integration_test)

add_executable(devices_cpu_bus_contract_integration_test
    integration/devices/cpu_bus_contract_integration_test.cpp
)
target_link_libraries(devices_cpu_bus_contract_integration_test PRIVATE sony_msx_core)
add_test(NAME devices_cpu_bus_contract_integration_test COMMAND devices_cpu_bus_contract_integration_test)

add_executable(boot_system_test
    system/boot_system_test.cpp
)
target_link_libraries(boot_system_test PRIVATE sony_msx_core)
add_test(NAME boot_system_test COMMAND boot_system_test)

add_executable(cpu_bootstrap_im1_system_test
    system/cpu_bootstrap_im1_system_test.cpp
)
target_link_libraries(cpu_bootstrap_im1_system_test PRIVATE sony_msx_core)
add_test(NAME cpu_bootstrap_im1_system_test COMMAND cpu_bootstrap_im1_system_test)

# --- M14 V9958 VDP (register/VRAM/status/interrupt contract) -----------------
add_executable(devices_vdp_vram_unit_test
    unit/devices/video_vdp_vram_unit_test.cpp
)
target_link_libraries(devices_vdp_vram_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_vram_unit_test COMMAND devices_vdp_vram_unit_test)

add_executable(devices_v9958_ports_unit_test
    unit/devices/video_v9958_ports_unit_test.cpp
)
target_link_libraries(devices_v9958_ports_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_ports_unit_test COMMAND devices_v9958_ports_unit_test)

add_executable(devices_v9958_registers_unit_test
    unit/devices/video_v9958_registers_unit_test.cpp
)
target_link_libraries(devices_v9958_registers_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_registers_unit_test COMMAND devices_v9958_registers_unit_test)

add_executable(devices_v9958_palette_mode_unit_test
    unit/devices/video_v9958_palette_mode_unit_test.cpp
)
target_link_libraries(devices_v9958_palette_mode_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_palette_mode_unit_test COMMAND devices_v9958_palette_mode_unit_test)

add_executable(devices_v9958_status_irq_unit_test
    unit/devices/video_v9958_status_irq_unit_test.cpp
)
target_link_libraries(devices_v9958_status_irq_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_status_irq_unit_test COMMAND devices_v9958_status_irq_unit_test)

# M36 Bug B: IE0/IE1 register-write /INT re-evaluation (the multi-disk RPG
# title's crash fix).
add_executable(devices_v9958_ie0_register_write_irq_unit_test
    unit/devices/video_v9958_ie0_register_write_irq_unit_test.cpp
)
target_link_libraries(devices_v9958_ie0_register_write_irq_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_ie0_register_write_irq_unit_test
    COMMAND devices_v9958_ie0_register_write_irq_unit_test)

add_executable(devices_vdp_vr_hr_raster_status_unit_test
    unit/devices/video_vdp_vr_hr_raster_status_unit_test.cpp
)
target_link_libraries(devices_vdp_vr_hr_raster_status_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_vr_hr_raster_status_unit_test COMMAND devices_vdp_vr_hr_raster_status_unit_test)

add_executable(machine_hbf1xv_vdp_io_integration_test
    integration/machine/hbf1xv_vdp_io_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_vdp_io_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_vdp_io_integration_test COMMAND machine_hbf1xv_vdp_io_integration_test)

add_executable(machine_hbf1xv_vdp_vr_hr_raster_integration_test
    integration/machine/hbf1xv_vdp_vr_hr_raster_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_vdp_vr_hr_raster_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_vdp_vr_hr_raster_integration_test COMMAND machine_hbf1xv_vdp_vr_hr_raster_integration_test)

# --- M15 device integration (PSG/RTC/PPI+keyboard/joystick/backup-RAM) --------
add_executable(devices_audio_psg_ym2149_unit_test
    unit/devices/audio_psg_ym2149_unit_test.cpp
)
target_link_libraries(devices_audio_psg_ym2149_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_psg_ym2149_unit_test COMMAND devices_audio_psg_ym2149_unit_test)

add_executable(peripherals_joystick_unit_test
    unit/peripherals/joystick_unit_test.cpp
)
target_link_libraries(peripherals_joystick_unit_test PRIVATE sony_msx_core)
add_test(NAME peripherals_joystick_unit_test COMMAND peripherals_joystick_unit_test)

add_executable(peripherals_keyboard_matrix_unit_test
    unit/peripherals/keyboard_matrix_unit_test.cpp
)
target_link_libraries(peripherals_keyboard_matrix_unit_test PRIVATE sony_msx_core)
add_test(NAME peripherals_keyboard_matrix_unit_test COMMAND peripherals_keyboard_matrix_unit_test)

add_executable(devices_rtc_rp5c01_unit_test
    unit/devices/rtc_rp5c01_unit_test.cpp
)
target_link_libraries(devices_rtc_rp5c01_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_rtc_rp5c01_unit_test COMMAND devices_rtc_rp5c01_unit_test)

add_executable(devices_chipset_ppi_8255_unit_test
    unit/devices/chipset_ppi_8255_unit_test.cpp
)
target_link_libraries(devices_chipset_ppi_8255_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_ppi_8255_unit_test COMMAND devices_chipset_ppi_8255_unit_test)

add_executable(devices_chipset_backup_ram_sram_unit_test
    unit/devices/chipset_backup_ram_sram_unit_test.cpp
)
target_link_libraries(devices_chipset_backup_ram_sram_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_backup_ram_sram_unit_test COMMAND devices_chipset_backup_ram_sram_unit_test)

add_executable(machine_hbf1xv_devices_integration_test
    integration/machine/hbf1xv_devices_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_devices_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_devices_integration_test
    COMMAND machine_hbf1xv_devices_integration_test)

add_executable(machine_hbf1xv_boot_checkpoint_devices_integration_test
    integration/machine/hbf1xv_boot_checkpoint_devices_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_boot_checkpoint_devices_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_boot_checkpoint_devices_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_boot_checkpoint_devices_integration_test
    COMMAND machine_hbf1xv_boot_checkpoint_devices_integration_test)

# --- M16 FDC (Fujitsu MB89311 / WD2793, Sony connection style) ---------------
add_executable(devices_fdc_disk_image_unit_test
    unit/devices/fdc/disk_image_unit_test.cpp
)
target_link_libraries(devices_fdc_disk_image_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_disk_image_unit_test COMMAND devices_fdc_disk_image_unit_test)

add_executable(devices_fdc_wd2793_type1_unit_test
    unit/devices/fdc/wd2793_type1_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_type1_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_type1_unit_test COMMAND devices_fdc_wd2793_type1_unit_test)

add_executable(devices_fdc_wd2793_type2_unit_test
    unit/devices/fdc/wd2793_type2_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_type2_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_type2_unit_test COMMAND devices_fdc_wd2793_type2_unit_test)

add_executable(devices_fdc_wd2793_type3_unit_test
    unit/devices/fdc/wd2793_type3_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_type3_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_type3_unit_test COMMAND devices_fdc_wd2793_type3_unit_test)

add_executable(devices_fdc_wd2793_type4_unit_test
    unit/devices/fdc/wd2793_type4_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_type4_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_type4_unit_test COMMAND devices_fdc_wd2793_type4_unit_test)

add_executable(devices_fdc_sony_fdc_unit_test
    unit/devices/fdc/sony_fdc_unit_test.cpp
)
target_link_libraries(devices_fdc_sony_fdc_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_sony_fdc_unit_test COMMAND devices_fdc_sony_fdc_unit_test)

add_executable(devices_fdc_wd2793_fastdisk_unit_test
    unit/devices/fdc/wd2793_fastdisk_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_fastdisk_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_fastdisk_unit_test COMMAND devices_fdc_wd2793_fastdisk_unit_test)

add_executable(devices_fdc_wd2793_write_stall_unit_test
    unit/devices/fdc/wd2793_write_stall_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_write_stall_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_write_stall_unit_test COMMAND devices_fdc_wd2793_write_stall_unit_test)

add_executable(devices_fdc_wd2793_write_firstbyte_unit_test
    unit/devices/fdc/wd2793_write_firstbyte_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_write_firstbyte_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_write_firstbyte_unit_test COMMAND devices_fdc_wd2793_write_firstbyte_unit_test)

add_executable(devices_fdc_wd2793_write_earlybyte_unit_test
    unit/devices/fdc/wd2793_write_earlybyte_unit_test.cpp
)
target_link_libraries(devices_fdc_wd2793_write_earlybyte_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_fdc_wd2793_write_earlybyte_unit_test COMMAND devices_fdc_wd2793_write_earlybyte_unit_test)

add_executable(machine_hbf1xv_fdc_integration_test
    integration/machine/hbf1xv_fdc_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_fdc_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_fdc_integration_test COMMAND machine_hbf1xv_fdc_integration_test)

add_executable(machine_hbf1xv_boot_checkpoint_fdc_integration_test
    integration/machine/hbf1xv_boot_checkpoint_fdc_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_boot_checkpoint_fdc_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_boot_checkpoint_fdc_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_boot_checkpoint_fdc_integration_test
    COMMAND machine_hbf1xv_boot_checkpoint_fdc_integration_test)

# --- M17 YM2413 (OPLL) register-accurate device + battery-backed SRAM primitive ---
add_executable(devices_audio_ym2413_opll_unit_test
    unit/devices/audio_ym2413_opll_unit_test.cpp
)
target_link_libraries(devices_audio_ym2413_opll_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_ym2413_opll_unit_test COMMAND devices_audio_ym2413_opll_unit_test)

add_executable(devices_memory_battery_backed_sram_unit_test
    unit/devices/memory_battery_backed_sram_unit_test.cpp
)
target_link_libraries(devices_memory_battery_backed_sram_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_memory_battery_backed_sram_unit_test COMMAND devices_memory_battery_backed_sram_unit_test)

add_executable(machine_hbf1xv_ym2413_integration_test
    integration/machine/hbf1xv_ym2413_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_ym2413_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_ym2413_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_ym2413_integration_test COMMAND machine_hbf1xv_ym2413_integration_test)

# --- M18 peripheral I/O: Kanji-font ROM (#D8-#DB) + printer (#90-#97) + cassette ---
add_executable(devices_kanji_font_rom_unit_test
    unit/devices/kanji/kanji_font_rom_unit_test.cpp
)
target_link_libraries(devices_kanji_font_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_kanji_font_rom_unit_test COMMAND devices_kanji_font_rom_unit_test)

add_executable(peripherals_printer_port_unit_test
    unit/peripherals/printer_port_unit_test.cpp
)
target_link_libraries(peripherals_printer_port_unit_test PRIVATE sony_msx_core)
add_test(NAME peripherals_printer_port_unit_test COMMAND peripherals_printer_port_unit_test)

add_executable(peripherals_cassette_interface_unit_test
    unit/peripherals/cassette_interface_unit_test.cpp
)
target_link_libraries(peripherals_cassette_interface_unit_test PRIVATE sony_msx_core)
add_test(NAME peripherals_cassette_interface_unit_test COMMAND peripherals_cassette_interface_unit_test)

add_executable(machine_hbf1xv_peripheral_io_integration_test
    integration/machine/hbf1xv_peripheral_io_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_peripheral_io_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_peripheral_io_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_peripheral_io_integration_test
    COMMAND machine_hbf1xv_peripheral_io_integration_test)

# --- M19 external cartridge slots 1/2 (RomBlocks/RomPlain/RomGeneric8kB/16kB/
#     RomAscii8kB/16kB/RomKonami MVP family, no SCC) -----------------------
add_executable(devices_cartridge_rom_window_unit_test
    unit/devices/cartridge/cartridge_rom_window_unit_test.cpp
)
target_link_libraries(devices_cartridge_rom_window_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_rom_window_unit_test COMMAND devices_cartridge_rom_window_unit_test)

add_executable(devices_cartridge_mapper_type_unit_test
    unit/devices/cartridge/cartridge_mapper_type_unit_test.cpp
)
target_link_libraries(devices_cartridge_mapper_type_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_mapper_type_unit_test COMMAND devices_cartridge_mapper_type_unit_test)

add_executable(devices_cartridge_mirrored_rom_unit_test
    unit/devices/cartridge/cartridge_mirrored_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_mirrored_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_mirrored_rom_unit_test COMMAND devices_cartridge_mirrored_rom_unit_test)

add_executable(devices_cartridge_generic8kb_rom_unit_test
    unit/devices/cartridge/cartridge_generic8kb_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_generic8kb_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_generic8kb_rom_unit_test COMMAND devices_cartridge_generic8kb_rom_unit_test)

add_executable(devices_cartridge_generic16kb_rom_unit_test
    unit/devices/cartridge/cartridge_generic16kb_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_generic16kb_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_generic16kb_rom_unit_test COMMAND devices_cartridge_generic16kb_rom_unit_test)

add_executable(devices_cartridge_ascii8kb_rom_unit_test
    unit/devices/cartridge/cartridge_ascii8kb_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_ascii8kb_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_ascii8kb_rom_unit_test COMMAND devices_cartridge_ascii8kb_rom_unit_test)

add_executable(devices_cartridge_ascii16kb_rom_unit_test
    unit/devices/cartridge/cartridge_ascii16kb_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_ascii16kb_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_ascii16kb_rom_unit_test COMMAND devices_cartridge_ascii16kb_rom_unit_test)

add_executable(devices_cartridge_konami_rom_unit_test
    unit/devices/cartridge/cartridge_konami_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_konami_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_konami_rom_unit_test COMMAND devices_cartridge_konami_rom_unit_test)

add_executable(devices_cartridge_slot_unit_test
    unit/devices/cartridge/cartridge_slot_unit_test.cpp
)
target_link_libraries(devices_cartridge_slot_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_slot_unit_test COMMAND devices_cartridge_slot_unit_test)

add_executable(machine_cartridge_cli_unit_test
    unit/machine/cartridge_cli_unit_test.cpp
)
target_link_libraries(machine_cartridge_cli_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_cartridge_cli_unit_test COMMAND machine_cartridge_cli_unit_test)

add_executable(machine_hbf1xv_cartridge_integration_test
    integration/machine/hbf1xv_cartridge_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cartridge_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_cartridge_integration_test
    COMMAND machine_hbf1xv_cartridge_integration_test)

set(SONY_MSX_ROMS_DIR_ABS "${CMAKE_SOURCE_DIR}/roms")

add_executable(machine_hbf1xv_scroll_shooter_smoke_integration_test
    integration/machine/hbf1xv_scroll_shooter_smoke_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_scroll_shooter_smoke_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_scroll_shooter_smoke_integration_test
    PRIVATE SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_scroll_shooter_smoke_integration_test
    COMMAND machine_hbf1xv_scroll_shooter_smoke_integration_test)

add_executable(machine_hbf1xv_cartridge_smoke_integration_test
    integration/machine/hbf1xv_cartridge_smoke_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cartridge_smoke_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_cartridge_smoke_integration_test
    PRIVATE SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_cartridge_smoke_integration_test
    COMMAND machine_hbf1xv_cartridge_smoke_integration_test)

# --- M20 Halnote / MSX-JE firmware mapping (slot 0-3) + real BatteryBackedSram
#     consumer (backlog B4 + B6, closed together) ---------------------------
add_executable(devices_halnote_rom_unit_test
    unit/devices/halnote/halnote_rom_unit_test.cpp
)
target_link_libraries(devices_halnote_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_halnote_rom_unit_test COMMAND devices_halnote_rom_unit_test)

add_executable(devices_halnote_subbank_unit_test
    unit/devices/halnote/halnote_subbank_unit_test.cpp
)
target_link_libraries(devices_halnote_subbank_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_halnote_subbank_unit_test COMMAND devices_halnote_subbank_unit_test)

add_executable(machine_hbf1xv_halnote_integration_test
    integration/machine/hbf1xv_halnote_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_halnote_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_halnote_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_halnote_integration_test
    COMMAND machine_hbf1xv_halnote_integration_test)

# --- M21 VDP rendering depth: pixel pipeline, YJK/YAE, scroll/interlace/blink,
#     G6/G7 planar interleave (backlog D1/D5/D6/D7-display-path) ------------
add_executable(devices_vdp_palette_unit_test
    unit/devices/video_vdp_palette_unit_test.cpp
)
target_link_libraries(devices_vdp_palette_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_palette_unit_test COMMAND devices_vdp_palette_unit_test)

add_executable(devices_vdp_frame_renderer_text1_unit_test
    unit/devices/video_vdp_frame_renderer_text1_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_text1_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_text1_unit_test
    COMMAND devices_vdp_frame_renderer_text1_unit_test)

add_executable(devices_vdp_frame_renderer_graphic1_unit_test
    unit/devices/video_vdp_frame_renderer_graphic1_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_graphic1_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_graphic1_unit_test
    COMMAND devices_vdp_frame_renderer_graphic1_unit_test)

add_executable(devices_vdp_frame_renderer_text2_unit_test
    unit/devices/video_vdp_frame_renderer_text2_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_text2_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_text2_unit_test
    COMMAND devices_vdp_frame_renderer_text2_unit_test)

add_executable(devices_vdp_frame_renderer_tile_modes_unit_test
    unit/devices/video_vdp_frame_renderer_tile_modes_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_tile_modes_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_tile_modes_unit_test
    COMMAND devices_vdp_frame_renderer_tile_modes_unit_test)

add_executable(devices_vdp_frame_renderer_bitmap_modes_unit_test
    unit/devices/video_vdp_frame_renderer_bitmap_modes_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_bitmap_modes_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_bitmap_modes_unit_test
    COMMAND devices_vdp_frame_renderer_bitmap_modes_unit_test)

add_executable(devices_vdp_planar_interleave_unit_test
    unit/devices/video_vdp_planar_interleave_unit_test.cpp
)
target_link_libraries(devices_vdp_planar_interleave_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_planar_interleave_unit_test
    COMMAND devices_vdp_planar_interleave_unit_test)

add_executable(devices_vdp_frame_renderer_yjk_unit_test
    unit/devices/video_vdp_frame_renderer_yjk_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_yjk_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_yjk_unit_test
    COMMAND devices_vdp_frame_renderer_yjk_unit_test)

add_executable(devices_vdp_frame_renderer_effects_unit_test
    unit/devices/video_vdp_frame_renderer_effects_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_effects_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_effects_unit_test
    COMMAND devices_vdp_frame_renderer_effects_unit_test)

add_executable(machine_hbf1xv_vdp_render_integration_test
    integration/machine/hbf1xv_vdp_render_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_vdp_render_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_vdp_render_integration_test
    COMMAND machine_hbf1xv_vdp_render_integration_test)

add_executable(vdp_render_system_test
    system/vdp_render_system_test.cpp
)
target_link_libraries(vdp_render_system_test PRIVATE sony_msx_core)
add_test(NAME vdp_render_system_test
    COMMAND vdp_render_system_test)

# --- M22 sprites (D2) + VDP command engine (D3), closes D7 in full --------
add_executable(devices_sprite_engine_mode1_unit_test
    unit/devices/video_sprite_engine_mode1_unit_test.cpp
)
target_link_libraries(devices_sprite_engine_mode1_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_sprite_engine_mode1_unit_test
    COMMAND devices_sprite_engine_mode1_unit_test)

add_executable(devices_sprite_engine_mode2_unit_test
    unit/devices/video_sprite_engine_mode2_unit_test.cpp
)
target_link_libraries(devices_sprite_engine_mode2_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_sprite_engine_mode2_unit_test
    COMMAND devices_sprite_engine_mode2_unit_test)

add_executable(devices_vdp_frame_renderer_sprites_unit_test
    unit/devices/video_vdp_frame_renderer_sprites_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_sprites_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_sprites_unit_test
    COMMAND devices_vdp_frame_renderer_sprites_unit_test)

# Sprite mode 2 R#5/R#11 attribute-table AND-mask semantics regression (the
# stealth-action-title sprite-invisibility fix,
# docs/sprite-invisibility-investigation.md).
add_executable(devices_sprite_engine_mode2_attribute_masking_unit_test
    unit/devices/video_sprite_engine_mode2_attribute_masking_unit_test.cpp
)
target_link_libraries(devices_sprite_engine_mode2_attribute_masking_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_sprite_engine_mode2_attribute_masking_unit_test
    COMMAND devices_sprite_engine_mode2_attribute_masking_unit_test)

# M49-S1 (backlog D9): progressive per-line LIVE sprite check (check_until).
# Byte-identity oracle -- incremental check_until() == the single-shot
# recompute_frame() sweep for a battery of static-VRAM/reg sprite configs (AC-S1).
add_executable(devices_sprite_engine_check_until_equivalence_unit_test
    unit/devices/video_sprite_engine_check_until_equivalence_unit_test.cpp
)
target_link_libraries(devices_sprite_engine_check_until_equivalence_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_sprite_engine_check_until_equivalence_unit_test
    COMMAND devices_sprite_engine_check_until_equivalence_unit_test)

# M49-S1: the ENGINE resolves sprite state per-line-live -- a mid-frame R#23 /
# R#1 BL / R#8 SPD rewrite fed directly to check_until() splits the frame (lines
# before the write use the old state, after use the new). S2 seam NOT exercised.
add_executable(devices_sprite_engine_per_line_state_unit_test
    unit/devices/video_sprite_engine_per_line_state_unit_test.cpp
)
target_link_libraries(devices_sprite_engine_per_line_state_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_sprite_engine_per_line_state_unit_test
    COMMAND devices_sprite_engine_per_line_state_unit_test)

add_executable(devices_vdp_command_engine_registers_unit_test
    unit/devices/video_vdp_command_engine_registers_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_registers_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_registers_unit_test
    COMMAND devices_vdp_command_engine_registers_unit_test)

add_executable(devices_vdp_command_engine_atomic_unit_test
    unit/devices/video_vdp_command_engine_atomic_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_atomic_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_atomic_unit_test
    COMMAND devices_vdp_command_engine_atomic_unit_test)

# M44 (DEF-M44-CMDSYNC Phase 2a, DEC-0069): the command-engine CE/S#2-bit0
# busy-wait DURATION -- pure-duration model + CE-window pacing + non-tautology
# + default-inertness (docs/m44-phase2a-planner-package.md §4.4).
add_executable(devices_vdp_command_engine_ce_duration_unit_test
    unit/devices/video_vdp_command_engine_ce_duration_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_ce_duration_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_ce_duration_unit_test
    COMMAND devices_vdp_command_engine_ce_duration_unit_test)

# M48 Slice 1 (DEC-0075): the sprite-aware tier-1 slot-availability factor
# (154/88/31 -> 1.00/1.75/4.97) that caps active-display command throughput,
# superseding the empirical kActiveDisplaySlotFactorPercent placeholder. Oracle
# re-derived from the tier-1 constants (non-tautological); byte-identity for the
# display-off path (docs/m48-planner-package.md §4.1 AC-1/AC-2/AC-5, EG-8).
add_executable(devices_v9958_cmd_slot_factor_unit_test
    unit/devices/video_v9958_cmd_slot_factor_unit_test.cpp
)
target_link_libraries(devices_v9958_cmd_slot_factor_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_cmd_slot_factor_unit_test
    COMMAND devices_v9958_cmd_slot_factor_unit_test)

# M48 Slice 2 (DEC-0075): the CPU-priority VRAM access-slot STEAL -- each CPU #98
# access during a busy command lengthens the command's CE window by one stolen
# slot = slot_cost_tstates(effective_slots_per_line()) T-states, ONE-DIRECTIONAL
# (the CPU is never stalled/dropped). Oracle re-derived from the tier-1 constants
# (2/3/8 T-states/access for 154/88/31), window-closed adversarial + device-level
# CPU-invariance + inert paths (docs/m48-planner-package.md §4.1 AC-3/AC-4/AC-5/
# AC-6, EG-8).
add_executable(devices_v9958_cmd_slot_steal_unit_test
    unit/devices/video_v9958_cmd_slot_steal_unit_test.cpp
)
target_link_libraries(devices_v9958_cmd_slot_steal_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_cmd_slot_steal_unit_test
    COMMAND devices_v9958_cmd_slot_steal_unit_test)

# VDP command-timing hardware-parity: the S#2 bit7 TR (transfer-ready) per-unit
# drop-then-rearm window for LMMC/HMMC/LMCM (deterministic, absolute-u64 clock).
add_executable(devices_vdp_command_engine_tr_pacing_unit_test
    unit/devices/video_vdp_command_engine_tr_pacing_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_tr_pacing_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_tr_pacing_unit_test
    COMMAND devices_vdp_command_engine_tr_pacing_unit_test)

add_executable(devices_vdp_command_engine_logic_ops_unit_test
    unit/devices/video_vdp_command_engine_logic_ops_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_logic_ops_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_logic_ops_unit_test
    COMMAND devices_vdp_command_engine_logic_ops_unit_test)

add_executable(devices_vdp_command_engine_transfer_unit_test
    unit/devices/video_vdp_command_engine_transfer_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_transfer_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_transfer_unit_test
    COMMAND devices_vdp_command_engine_transfer_unit_test)

add_executable(devices_vdp_command_engine_nonbitmap_unit_test
    unit/devices/video_vdp_command_engine_nonbitmap_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_nonbitmap_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_nonbitmap_unit_test
    COMMAND devices_vdp_command_engine_nonbitmap_unit_test)

add_executable(machine_hbf1xv_sprites_integration_test
    integration/machine/hbf1xv_sprites_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_sprites_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_sprites_integration_test
    COMMAND machine_hbf1xv_sprites_integration_test)

add_executable(machine_hbf1xv_command_engine_integration_test
    integration/machine/hbf1xv_command_engine_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_command_engine_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_command_engine_integration_test
    COMMAND machine_hbf1xv_command_engine_integration_test)

add_executable(sprites_command_engine_system_test
    system/sprites_command_engine_system_test.cpp
)
target_link_libraries(sprites_command_engine_system_test PRIVATE sony_msx_core)
add_test(NAME sprites_command_engine_system_test
    COMMAND sprites_command_engine_system_test)

# --- M23 Z80 HALT-R closure (C2, DONE) + VDP access-timing foundation --------
# (C1/D4, narrowed/non-gating) --------------------------------------------
add_executable(devices_z80a_halt_r_unit_test
    unit/devices/z80a_halt_r_unit_test.cpp
)
target_link_libraries(devices_z80a_halt_r_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_halt_r_unit_test COMMAND devices_z80a_halt_r_unit_test)

add_executable(machine_hbf1xv_halt_r_integration_test
    integration/machine/hbf1xv_halt_r_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_halt_r_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_halt_r_integration_test
    COMMAND machine_hbf1xv_halt_r_integration_test)

add_executable(devices_video_vdp_access_timing_unit_test
    unit/devices/video_vdp_access_timing_unit_test.cpp
)
target_link_libraries(devices_video_vdp_access_timing_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_video_vdp_access_timing_unit_test
    COMMAND devices_video_vdp_access_timing_unit_test)

add_executable(machine_hbf1xv_vdp_access_timing_non_gating_integration_test
    integration/machine/hbf1xv_vdp_access_timing_non_gating_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_vdp_access_timing_non_gating_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_vdp_access_timing_non_gating_integration_test
    COMMAND machine_hbf1xv_vdp_access_timing_non_gating_integration_test)

# M48 Slice 2 (DEC-0075): CPU-timing invariance for the CPU-priority slot steal,
# proven in the REAL machine path (real Z80 + I/O bus + scheduler). Two
# byte-identical programs (differing only in the R#46 command operand) run in
# lockstep; per-instruction AND cumulative T-states must be identical whether the
# steal is active (HMMV busy) or inert (STOP) -- the AC-4 machine-path witness --
# and the busy program's CE window is proven to grow by exactly K*slot_cost(154)
# (docs/m48-planner-package.md §3 Slice-2 DoD, §4.1 AC-4, §6 R1).
add_executable(machine_hbf1xv_cpu_vram_steal_invariance_integration_test
    integration/machine/hbf1xv_cpu_vram_steal_invariance_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cpu_vram_steal_invariance_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_cpu_vram_steal_invariance_integration_test
    COMMAND machine_hbf1xv_cpu_vram_steal_invariance_integration_test)

# --- M24 ZEXDOC/ZEXALL full parity sweep (backlog C3): generic CP/M/BDOS ----
#     harness (src/machine/cpm_bdos_harness.*) + the real GPL exerciser
#     system test. The harness's own unit tests use ONLY a hand-written
#     synthetic fixture (never the real binaries, planner package SS1.5/
#     Acceptance Criterion 2); the real src/external/zexall/*.com binaries are
#     loaded ONLY by the dedicated system test below. -----------------------
add_executable(machine_cpm_bdos_harness_unit_test
    unit/machine/cpm_bdos_harness_unit_test.cpp
)
target_link_libraries(machine_cpm_bdos_harness_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_cpm_bdos_harness_unit_test COMMAND machine_cpm_bdos_harness_unit_test)

add_executable(machine_hbf1xv_cpm_run_integration_test
    integration/machine/hbf1xv_cpm_run_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cpm_run_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_cpm_run_integration_test
    COMMAND machine_hbf1xv_cpm_run_integration_test)

# Absolute path to the repo src/external/zexall/ directory (mirrors the
# established SONY_MSX_BIOS_DIR_ABS pattern above, per DEC-0016's standing
# watch-item: a test needing a real asset path MUST wire its own compile
# definition or it silently 0xFF-fills/degrades under ctest's differing
# working directory).
set(SONY_MSX_ZEXALL_DIR_ABS "${CMAKE_SOURCE_DIR}/src/external/zexall")

# NOTE (developer disclosure, M24-S3): this system test runs BOTH real GPL
# exerciser binaries (src/external/zexall/zexall.com, zexdoc.com) to genuine
# CP/M warm-boot completion, in-process, via CpmBdosHarness. Empirically
# measured wall-clock cost is on the order of tens of minutes total (see
# docs/m24-implementation-report.md for the actual measured figure) -- this
# materially lengthens a full `ctest` invocation. This is a disclosed,
# intentional consequence of Acceptance Criterion 3 (a genuine, non-truncated
# full-suite run), not an oversight; see the implementation report's runtime
# section for the full honest disclosure and the coordinator/QA escalation
# this milestone's planner package anticipated (R-M24-3).
add_executable(zexall_system_test
    system/zexall_system_test.cpp
)
target_link_libraries(zexall_system_test PRIVATE sony_msx_core)
target_compile_definitions(zexall_system_test
    PRIVATE SONY_MSX_ZEXALL_DIR="${SONY_MSX_ZEXALL_DIR_ABS}")
add_test(NAME zexall_system_test COMMAND zexall_system_test)
set_tests_properties(zexall_system_test PROPERTIES
    TIMEOUT 7200
    LABELS "slow_cpu_sweep")

# --- M25 Sony speed-controller (MB670836) + hardware PAUSE + Ren-Sha Turbo --
#     autofire (backlog C8) ---------------------------------------------------
add_executable(devices_chipset_mb670836_pause_unit_test
    unit/devices/chipset/mb670836_pause_unit_test.cpp
)
target_link_libraries(devices_chipset_mb670836_pause_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_mb670836_pause_unit_test COMMAND devices_chipset_mb670836_pause_unit_test)

add_executable(peripherals_rensha_turbo_unit_test
    unit/peripherals/rensha_turbo_unit_test.cpp
)
target_link_libraries(peripherals_rensha_turbo_unit_test PRIVATE sony_msx_core)
add_test(NAME peripherals_rensha_turbo_unit_test COMMAND peripherals_rensha_turbo_unit_test)

add_executable(peripherals_rensha_turbo_integration_test
    integration/peripherals/rensha_turbo_integration_test.cpp
)
target_link_libraries(peripherals_rensha_turbo_integration_test PRIVATE sony_msx_core)
add_test(NAME peripherals_rensha_turbo_integration_test COMMAND peripherals_rensha_turbo_integration_test)

add_executable(machine_hbf1xv_pause_integration_test
    integration/machine/hbf1xv_pause_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_pause_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_pause_integration_test COMMAND machine_hbf1xv_pause_integration_test)

add_executable(machine_hbf1xv_speed_controller_integration_test
    integration/machine/hbf1xv_speed_controller_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_speed_controller_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_speed_controller_integration_test
    COMMAND machine_hbf1xv_speed_controller_integration_test)

add_executable(speed_pause_rensha_system_test
    system/speed_pause_rensha_system_test.cpp
)
target_link_libraries(speed_pause_rensha_system_test PRIVATE sony_msx_core)
add_test(NAME speed_pause_rensha_system_test
    COMMAND speed_pause_rensha_system_test)

# --- M26 SDL3 frontend (backlog C9): windowing/real-time loop, video/audio
#     presentation, input mapping, decoded-FrameBuffer PNG capture --------
# S1 (on_vsync_boundary() seam) and S4 (frame-dump capability) are headless-
# buildable and registered here, OUTSIDE the SONY_MSX_ENABLE_SDL3 guard below
# (docs/m26-planner-package.md §3, M26-S1/S4) -- the default headless ctest
# suite gains exactly these two new cases; every other M26 test requires a
# real SDL3 build and lives in the guarded block further down.
add_executable(machine_hbf1xv_vsync_boundary_integration_test
    integration/machine/hbf1xv_vsync_boundary_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_vsync_boundary_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_vsync_boundary_integration_test
    COMMAND machine_hbf1xv_vsync_boundary_integration_test)

add_executable(machine_frame_dump_unit_test
    unit/machine/frame_dump_unit_test.cpp
)
target_link_libraries(machine_frame_dump_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_frame_dump_unit_test COMMAND machine_frame_dump_unit_test)

add_executable(frontend_psg_audio_pump_unit_test
    unit/frontend/psg_audio_pump_unit_test.cpp
)
target_link_libraries(frontend_psg_audio_pump_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_psg_audio_pump_unit_test COMMAND frontend_psg_audio_pump_unit_test)

# Audio-latency fix (docs/audio-latency-investigation.md): SDL-free exact-
# accounting + backpressure pacing policy -- headlessly provable, mirrors the
# PsgAudioPump SDL3-independence precedent.
add_executable(frontend_audio_pacer_unit_test
    unit/frontend/audio_pacer_unit_test.cpp
)
target_link_libraries(frontend_audio_pacer_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_audio_pacer_unit_test COMMAND frontend_audio_pacer_unit_test)

add_executable(frontend_sdl3_cli_unit_test
    unit/frontend/sdl3_cli_unit_test.cpp
)
target_link_libraries(frontend_sdl3_cli_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_sdl3_cli_unit_test COMMAND frontend_sdl3_cli_unit_test)

# M46 (DEC-0071): pure banner-formatting helpers (RAM/mode-tag/FM-PAC/SRAM
# lines) -- headless-buildable, SDL-free (AC-14 ctest seam).
add_executable(frontend_session_summary_unit_test
    unit/frontend/session_summary_unit_test.cpp
)
target_link_libraries(frontend_session_summary_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_session_summary_unit_test COMMAND frontend_session_summary_unit_test)

# --- M27 Production Hardening + Debug/Test Tooling: full state-dump/CPU-
#     trace/event-log CLI wiring, real decoded PSG audio capture, keystroke-
#     sequencing/scripted-input automation, and the replay-determinism proof
#     (docs/m27-planner-package.md). Headless-buildable; registered OUTSIDE
#     the SONY_MSX_ENABLE_SDL3 guard below (mirrors the M26-S1/S4 precedent).
add_executable(peripherals_msx_key_names_unit_test
    unit/peripherals/msx_key_names_unit_test.cpp
)
target_link_libraries(peripherals_msx_key_names_unit_test PRIVATE sony_msx_core)
add_test(NAME peripherals_msx_key_names_unit_test COMMAND peripherals_msx_key_names_unit_test)

add_executable(machine_input_script_unit_test
    unit/machine/input_script_unit_test.cpp
)
target_link_libraries(machine_input_script_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_input_script_unit_test COMMAND machine_input_script_unit_test)

# Input RECORDER (DEC-0072): the write-side counterpart to the input-script
# parser/player -- serialize_input_script_line() single-source byte format,
# InputScriptRecorder streaming round-trip, and '#'-comment skipping. Headless
# (SDL3-independent), so it runs in the default ctest suite.
add_executable(machine_input_recorder_unit_test
    unit/machine/input_recorder_unit_test.cpp
)
target_link_libraries(machine_input_recorder_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_input_recorder_unit_test COMMAND machine_input_recorder_unit_test)

add_executable(frontend_psg_audio_dump_unit_test
    unit/frontend/psg_audio_dump_unit_test.cpp
)
target_link_libraries(frontend_psg_audio_dump_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_psg_audio_dump_unit_test COMMAND frontend_psg_audio_dump_unit_test)

add_executable(machine_hbf1xv_debug_session_integration_test
    integration/machine/hbf1xv_debug_session_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_debug_session_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_debug_session_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_DISKS_DIR="${CMAKE_SOURCE_DIR}/disks")
add_test(NAME machine_hbf1xv_debug_session_integration_test
    COMMAND machine_hbf1xv_debug_session_integration_test)

add_executable(machine_hbf1xv_input_script_integration_test
    integration/machine/hbf1xv_input_script_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_input_script_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_input_script_integration_test
    COMMAND machine_hbf1xv_input_script_integration_test)

# M41-S1: JOY= input-script verb -> JoystickPorts -> PSG R14 read-path proof.
add_executable(machine_input_script_joystick_integration_test
    integration/machine/input_script_joystick_integration_test.cpp
)
target_link_libraries(machine_input_script_joystick_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_input_script_joystick_integration_test
    COMMAND machine_input_script_joystick_integration_test)

add_executable(replay_determinism_system_test
    system/replay_determinism_system_test.cpp
)
target_link_libraries(replay_determinism_system_test PRIVATE sony_msx_core)
target_compile_definitions(replay_determinism_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME replay_determinism_system_test COMMAND replay_determinism_system_test)

add_executable(debug_tooling_system_test
    system/debug_tooling_system_test.cpp
)
target_link_libraries(debug_tooling_system_test PRIVATE sony_msx_core)
target_compile_definitions(debug_tooling_system_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME debug_tooling_system_test COMMAND debug_tooling_system_test)

# --- M29 KonamiSCC mapper + SCC wavetable audio chip (backlog G1, docs/
#     m29-planner-package.md): the SccWavetable device unit suite (De
#     Schrijder/Pazos/NYYRIKKI hand-computed oracles), the CartridgeKonamiScc
#     mapper unit suite (opposite-of-plain-Konami mirroring, masked 0x3F
#     enable, both-effects 0x9000 write, 0x9800-0x9FFF window), the
#     MachineAudioMixer unit suite (incl. the zero-SCC byte-identity hard
#     regression oracle), the bus-level integration test (real Z80 writes
#     over the M11 bus), and the cold-boot in-cart-driver system test. All
#     headless-buildable (the mixer is SDL3-independent, M26-S5 precedent).
add_executable(devices_audio_scc_wavetable_unit_test
    unit/devices/audio_scc_wavetable_unit_test.cpp
)
target_link_libraries(devices_audio_scc_wavetable_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_scc_wavetable_unit_test COMMAND devices_audio_scc_wavetable_unit_test)

# --- M34 playtest defect pair (DEC-0043, docs/m34-planner-package.md):
#     Defect A = PSG/SCC box-average integration (the shared rounding helper
#     + chip-level dwell oracles extend the existing chip unit tests above;
#     the new mixer-level ultrasonic/mid-band oracles and the scroll-shooter
#     regression live further below); Defect B = the R#1 BL render gate
#     (renderer BL unit test + L+1 latch integration test below).
add_executable(devices_audio_dwell_rounding_unit_test
    unit/devices/audio_dwell_rounding_unit_test.cpp
)
target_link_libraries(devices_audio_dwell_rounding_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_dwell_rounding_unit_test COMMAND devices_audio_dwell_rounding_unit_test)

# --- M39-A digitized-voice fix: 1-bit key-click DAC + PSG sync-before-change ---
add_executable(devices_audio_click_dac_unit_test
    unit/devices/audio_click_dac_unit_test.cpp
)
target_link_libraries(devices_audio_click_dac_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_click_dac_unit_test COMMAND devices_audio_click_dac_unit_test)

add_executable(devices_audio_psg_sync_before_change_unit_test
    unit/devices/audio_psg_sync_before_change_unit_test.cpp
)
target_link_libraries(devices_audio_psg_sync_before_change_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_psg_sync_before_change_unit_test
    COMMAND devices_audio_psg_sync_before_change_unit_test)

add_executable(devices_cartridge_konami_scc_rom_unit_test
    unit/devices/cartridge/cartridge_konami_scc_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_konami_scc_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_konami_scc_rom_unit_test COMMAND devices_cartridge_konami_scc_rom_unit_test)

add_executable(frontend_machine_audio_mixer_unit_test
    unit/frontend/machine_audio_mixer_unit_test.cpp
)
target_link_libraries(frontend_machine_audio_mixer_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_audio_mixer_unit_test COMMAND frontend_machine_audio_mixer_unit_test)

# M39-A: 1-bit key-click DAC wired into the mixer as a fifth additive source.
add_executable(frontend_machine_audio_mixer_click_unit_test
    unit/frontend/machine_audio_mixer_click_unit_test.cpp
)
target_link_libraries(frontend_machine_audio_mixer_click_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_audio_mixer_click_unit_test
    COMMAND frontend_machine_audio_mixer_click_unit_test)

add_executable(machine_hbf1xv_konami_scc_integration_test
    integration/machine/hbf1xv_konami_scc_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_konami_scc_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_konami_scc_integration_test
    COMMAND machine_hbf1xv_konami_scc_integration_test)

# --- M36 (DEC-0050): FM-PAC peripheral cartridge + disk-save persistence -----
add_executable(devices_cartridge_fmpac_rom_unit_test
    unit/devices/cartridge/cartridge_fmpac_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_fmpac_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_fmpac_rom_unit_test COMMAND devices_cartridge_fmpac_rom_unit_test)

# M43 Slice 3 (DEC-0062/0063): FM-PAC battery-SRAM openMSX `.sram` format +
# lossless legacy-raw-8192 migration + fail-safe rejection round-trips.
add_executable(devices_cartridge_fmpac_sram_persistence_unit_test
    unit/devices/cartridge/cartridge_fmpac_sram_persistence_unit_test.cpp
)
target_link_libraries(devices_cartridge_fmpac_sram_persistence_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_fmpac_sram_persistence_unit_test
    COMMAND devices_cartridge_fmpac_sram_persistence_unit_test)

add_executable(devices_disk_image_writeback_unit_test
    unit/devices/fdc/disk_image_writeback_unit_test.cpp
)
target_link_libraries(devices_disk_image_writeback_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_disk_image_writeback_unit_test COMMAND devices_disk_image_writeback_unit_test)

add_executable(machine_hbf1xv_fmpac_cartridge_integration_test
    integration/machine/hbf1xv_fmpac_cartridge_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_fmpac_cartridge_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_fmpac_cartridge_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
            SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_fmpac_cartridge_integration_test
    COMMAND machine_hbf1xv_fmpac_cartridge_integration_test)

add_executable(machine_hbf1xv_disk_save_persist_integration_test
    integration/machine/hbf1xv_disk_save_persist_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_disk_save_persist_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_disk_save_persist_integration_test
    COMMAND machine_hbf1xv_disk_save_persist_integration_test)

add_executable(scc_system_test
    system/scc_system_test.cpp
)
target_link_libraries(scc_system_test PRIVATE sony_msx_core)
add_test(NAME scc_system_test COMMAND scc_system_test)

# --- M30 universal cartridge auto-identification (backlog G2, docs/
#     m30-planner-package.md): clean-room SHA-1 (published FIPS/RFC vectors),
#     the tolerant softwaredb subset parser (FULLY SYNTHETIC fixtures --
#     never real softwaredb content), the precedence engine + re-derived
#     bank-write heuristic + verbatim A-E messages, and the asset-gated
#     real-ROM identification/boot-smoke integration test. All headless-
#     buildable; the SDL3-side additive case lives in the guarded
#     frontend_sdl3_cli_session_integration_test below.
add_executable(machine_sha1_unit_test
    unit/machine/sha1_unit_test.cpp
)
target_link_libraries(machine_sha1_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_sha1_unit_test COMMAND machine_sha1_unit_test)

# M50-S1 (DEC-0077): shared XML tokenizer extracted from software_db.cpp
# (xml_tokenizer.{h,cpp}) + the externalized-config parser/validator
# (emulator_config.{h,cpp}). The tokenizer test pins refactor equivalence
# (capture on/off same trace) + attribute extraction; the config test is the
# non-tautological parse/validate/fallback suite (docs/m50-planner-package.md §7).
add_executable(machine_xml_tokenizer_unit_test
    unit/machine/xml_tokenizer_unit_test.cpp
)
target_link_libraries(machine_xml_tokenizer_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_xml_tokenizer_unit_test COMMAND machine_xml_tokenizer_unit_test)

add_executable(machine_emulator_config_unit_test
    unit/machine/emulator_config_unit_test.cpp
)
target_link_libraries(machine_emulator_config_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_emulator_config_unit_test COMMAND machine_emulator_config_unit_test)

add_executable(machine_software_db_unit_test
    unit/machine/software_db_unit_test.cpp
)
target_link_libraries(machine_software_db_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_software_db_unit_test COMMAND machine_software_db_unit_test)

# M50-S2 (DEC-0077): the CLI-default RESOLUTION seam (config_runtime.{h,cpp}) --
# precedence CLI > XML > built-in default + the determinism auto-load gate.
# SDL-free (links sony_msx_core), so registered OUTSIDE the SONY_MSX_ENABLE_SDL3
# guard. The unit test proves the gate + per-knob precedence with concrete
# (non-tautological) values; the integration test drives the load/search/warning
# wiring + the determinism proof against real on-disk config files.
add_executable(frontend_config_runtime_unit_test
    unit/frontend/config_runtime_unit_test.cpp
)
target_link_libraries(frontend_config_runtime_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_config_runtime_unit_test COMMAND frontend_config_runtime_unit_test)

add_executable(frontend_config_runtime_wiring_integration_test
    integration/frontend/config_runtime_wiring_integration_test.cpp
)
target_link_libraries(frontend_config_runtime_wiring_integration_test PRIVATE sony_msx_core)
add_test(NAME frontend_config_runtime_wiring_integration_test
    COMMAND frontend_config_runtime_wiring_integration_test)

# M52 (DEC-0079): the SDL-free master-volume gain/step helper (master_volume.h)
# + the disk-writable Alt+S toggle flush contract. All three link sony_msx_core
# (SDL-free -- master_volume.h, MachineAudioMixer, Hbf1xvMachine + DiskImage are
# headlessly testable), so they register OUTSIDE the SONY_MSX_ENABLE_SDL3 guard.
# The unit test targets the pure gain/step math (identity@100 byte-oracle, silence
# @0, mid attenuation, clamp-not-wrap step); the audio-identity integration test
# proves the master gain is provably post-mix + byte-identical at full volume
# WITHOUT an audio device; the toggle test proves the Alt+S bind/unbind flush
# semantics (ON persists, OFF discards + no rollback, late-ON flushes whole image).
add_executable(frontend_master_volume_unit_test
    unit/frontend/master_volume_unit_test.cpp
)
target_link_libraries(frontend_master_volume_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_master_volume_unit_test COMMAND frontend_master_volume_unit_test)

add_executable(frontend_master_volume_audio_identity_integration_test
    integration/frontend/master_volume_audio_identity_integration_test.cpp
)
target_link_libraries(frontend_master_volume_audio_identity_integration_test PRIVATE sony_msx_core)
add_test(NAME frontend_master_volume_audio_identity_integration_test
    COMMAND frontend_master_volume_audio_identity_integration_test)

add_executable(frontend_disk_writable_toggle_flush_integration_test
    integration/frontend/disk_writable_toggle_flush_integration_test.cpp
)
target_link_libraries(frontend_disk_writable_toggle_flush_integration_test PRIVATE sony_msx_core)
add_test(NAME frontend_disk_writable_toggle_flush_integration_test
    COMMAND frontend_disk_writable_toggle_flush_integration_test)

# M55 (DEC-0083): the SDL-free / ImGui-free menu MODEL (menu_model.h, compiled
# into sony_msx_core). Proves the complete v1 tree vs the DEC-0083 layout, the
# grayed star set, checkmark/radio bindings, the single-sourced hotkey table, and
# the pure event-capture predicate -- all headlessly, so it registers OUTSIDE the
# SONY_MSX_ENABLE_SDL3 guard (the established SDL-free-frontend-logic pattern).
add_executable(frontend_menu_model_unit_test
    unit/frontend/menu_model_unit_test.cpp
)
target_link_libraries(frontend_menu_model_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_menu_model_unit_test COMMAND frontend_menu_model_unit_test)

# M57 (DEC-0085, §4.3): the PERMANENT DEF-2 inset-geometry oracle. Pure SDL-free
# letterbox_into_band() fit math (frontend/letterbox_geometry.h, header-only), so
# it registers OUTSIDE the SONY_MSX_ENABLE_SDL3 guard and counts in BOTH the ON
# and OFF denominators.
add_executable(frontend_letterbox_geometry_unit_test
    unit/frontend/letterbox_geometry_unit_test.cpp
)
target_link_libraries(frontend_letterbox_geometry_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_letterbox_geometry_unit_test COMMAND frontend_letterbox_geometry_unit_test)

# M61 (DEC-0090): the PERMANENT display-fit oracle. Pure SDL-free
# fit_window_to_display() math (frontend/window_fit.h, header-only) that keeps
# the interactive window from opening larger than the display's usable bounds
# (owner Pi 4B 800x480 panel), plus its hand-off contract with the M57
# letterbox_into_band() at clamped sizes. Registers OUTSIDE the
# SONY_MSX_ENABLE_SDL3 guard and counts in BOTH the ON and OFF denominators.
add_executable(frontend_window_fit_unit_test
    unit/frontend/window_fit_unit_test.cpp
)
target_link_libraries(frontend_window_fit_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_window_fit_unit_test COMMAND frontend_window_fit_unit_test)

# DEC-0095: the settings-persistence emitter's anti-drift oracle -- successor to
# the retired repo-root shipped-config round-trip test (see the note above).
# SDL-free (pure string/config math) -> registered OUTSIDE the SONY_MSX_ENABLE_SDL3
# guard, so it counts in BOTH the ON and OFF denominators.
add_executable(frontend_config_xml_writer_unit_test
    unit/frontend/config_xml_writer_unit_test.cpp
)
target_link_libraries(frontend_config_xml_writer_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_config_xml_writer_unit_test COMMAND frontend_config_xml_writer_unit_test)

# DEC-0095-AMENDMENT-D: guards the persisted-asset-path absolutization that fixes
# FM-PAC auto-load silently skipping (empty slot 2) when the settings file beside
# the exe carried relative paths. Pure/injected base dir -> SDL-free, registered
# OUTSIDE the SONY_MSX_ENABLE_SDL3 guard.
add_executable(frontend_config_paths_unit_test
    unit/frontend/config_paths_unit_test.cpp
)
target_link_libraries(frontend_config_paths_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_config_paths_unit_test COMMAND frontend_config_paths_unit_test)

# M63 (generalized at M64): the dialog default-directory policy oracle shared
# by ALL the in-window dialog launchers. Pure filesystem-free
# choose_dialog_dir() (frontend/dialog_default_dir.h, header-only; the
# "exists && is_directory" verdict is injected). Registers OUTSIDE the
# SONY_MSX_ENABLE_SDL3 guard and counts in BOTH denominators.
add_executable(frontend_dialog_default_dir_unit_test
    unit/frontend/dialog_default_dir_unit_test.cpp
)
target_link_libraries(frontend_dialog_default_dir_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_dialog_default_dir_unit_test COMMAND frontend_dialog_default_dir_unit_test)

# M56 (DEC-0084): SDL-free frontend-logic tests -- the async-dialog classifier
# (dialog_result.h), the F5 blank-disk writer + golden SHA (blank_disk_image.h,
# compiled into sony_msx_core), and the F1 disk-replace dirty-flush semantics
# (Hbf1xvMachine + DiskImage). All link sony_msx_core and register OUTSIDE the
# SONY_MSX_ENABLE_SDL3 guard (the established SDL-free-frontend-logic pattern), so
# they count in BOTH the ON and OFF denominators.
add_executable(frontend_dialog_mailbox_unit_test
    unit/frontend/dialog_mailbox_unit_test.cpp
)
target_link_libraries(frontend_dialog_mailbox_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_dialog_mailbox_unit_test COMMAND frontend_dialog_mailbox_unit_test)

add_executable(frontend_blank_disk_image_unit_test
    unit/frontend/blank_disk_image_unit_test.cpp
)
target_link_libraries(frontend_blank_disk_image_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_blank_disk_image_unit_test COMMAND frontend_blank_disk_image_unit_test)

add_executable(frontend_disk_replace_flush_semantics_integration_test
    integration/frontend/disk_replace_flush_semantics_integration_test.cpp
)
target_link_libraries(frontend_disk_replace_flush_semantics_integration_test PRIVATE sony_msx_core)
add_test(NAME frontend_disk_replace_flush_semantics_integration_test
    COMMAND frontend_disk_replace_flush_semantics_integration_test)

# M50-S3 (DEC-0077): the MACHINE-sizing/path RESOLUTION seam
# (resolve_machine_config -- BIOS dir + 7 filenames, FM-PAC ROM/SRAM, softwaredb,
# VRAM validated-to-128) + the machine-wiring proof that config-fed BIOS
# filenames flow into RomAssetLoader. Both SDL-free (link sony_msx_core). The
# unit test proves CLI > XML > default precedence with concrete asserted values;
# the integration test drives an empty asset root + reads the loader diagnostics.
add_executable(frontend_machine_config_resolution_unit_test
    unit/frontend/machine_config_resolution_unit_test.cpp
)
target_link_libraries(frontend_machine_config_resolution_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_config_resolution_unit_test
    COMMAND frontend_machine_config_resolution_unit_test)

add_executable(machine_hbf1xv_bios_filename_config_integration_test
    integration/machine/hbf1xv_bios_filename_config_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_bios_filename_config_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_bios_filename_config_integration_test
    COMMAND machine_hbf1xv_bios_filename_config_integration_test)

# M50-S4 (DEC-0077): the SHIPPED-CONFIG round-trip ANTI-DRIFT guard
# (docs/m50-planner-package.md §4.7 / AC-S4). Parses the TRACKED reference
# sony_msx_hbf1xv.xml at the repo ROOT and asserts it is byte-identical to the
# compiled built-in defaults with ZERO warnings (and documents every knob). The
# repo-root path is injected as an absolute compile def so the test is
# cwd-independent. FAILS if the shipped file ever drifts from the code defaults.
# DEC-0095-AMENDMENT-C: the file-based repo-root shipped-config round-trip test
# was RETIRED here. There is no longer a shipped config at the repository root --
# v1.6.0 has the frontend WRITE sony_msx_hbf1xv.xml beside the executable -- so
# the anti-drift invariant moved in-memory (and got stronger: it now covers
# arbitrary configs rather than one shipped file). Successor:
# frontend_config_xml_writer_unit_test, registered with the frontend tests below.

add_executable(machine_cartridge_identifier_unit_test
    unit/machine/cartridge_identifier_unit_test.cpp
)
target_link_libraries(machine_cartridge_identifier_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_cartridge_identifier_unit_test COMMAND machine_cartridge_identifier_unit_test)

# The DEC-0016 absolute-path compile-def pattern (see the watch-item note
# above): the real softwaredb.xml stays in references/ untouched -- the test
# only PARSES it at runtime, and skip-gates on its presence (plus on the ROM
# asset's presence AND exact SHA1, planner A-M30-1/A-M30-2).
set(SONY_MSX_SOFTWAREDB_PATH_ABS "${CMAKE_SOURCE_DIR}/references/openmsx-21.0/share/softwaredb.xml")

add_executable(machine_hbf1xv_cartridge_identification_integration_test
    integration/machine/hbf1xv_cartridge_identification_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_cartridge_identification_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_cartridge_identification_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}"
    SONY_MSX_SOFTWAREDB_PATH="${SONY_MSX_SOFTWAREDB_PATH_ABS}")
add_test(NAME machine_hbf1xv_cartridge_identification_integration_test
    COMMAND machine_hbf1xv_cartridge_identification_integration_test)

# --- M31 YM2413 FM-synthesis DSP depth (backlog E1, docs/m31-planner-
#     package.md, RELEASE CANDIDATE): the formula-grounded Ym2413Synth unit
#     suites (S1 closed-form table oracles recomputed independently in-test;
#     S2 pitch/volume/feedback/half-sine oracles; S3 §5 decay/release exact
#     duration oracles + the disclosed attack approximation's qualitative
#     contract; S4 rhythm x2 gain law / keying / AM-VIB laws), the mixer
#     three-source suite (incl. the zero-YM2413 byte-identity HARD oracle
#     and the 9:8 decimation proof), the real-title FM probe + SCC-cartridge
#     evidence integration tests (asset-gated skips), and the register-
#     driven CPU-OUT system test. All headless-buildable.
add_executable(devices_audio_ym2413_synth_tables_unit_test
    unit/devices/audio_ym2413_synth_tables_unit_test.cpp
)
target_link_libraries(devices_audio_ym2413_synth_tables_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_ym2413_synth_tables_unit_test
    COMMAND devices_audio_ym2413_synth_tables_unit_test)

add_executable(devices_audio_ym2413_synth_operator_unit_test
    unit/devices/audio_ym2413_synth_operator_unit_test.cpp
)
target_link_libraries(devices_audio_ym2413_synth_operator_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_ym2413_synth_operator_unit_test
    COMMAND devices_audio_ym2413_synth_operator_unit_test)

add_executable(devices_audio_ym2413_synth_envelope_unit_test
    unit/devices/audio_ym2413_synth_envelope_unit_test.cpp
)
target_link_libraries(devices_audio_ym2413_synth_envelope_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_ym2413_synth_envelope_unit_test
    COMMAND devices_audio_ym2413_synth_envelope_unit_test)

add_executable(devices_audio_ym2413_synth_rhythm_lfo_unit_test
    unit/devices/audio_ym2413_synth_rhythm_lfo_unit_test.cpp
)
target_link_libraries(devices_audio_ym2413_synth_rhythm_lfo_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_ym2413_synth_rhythm_lfo_unit_test
    COMMAND devices_audio_ym2413_synth_rhythm_lfo_unit_test)

add_executable(frontend_machine_audio_mixer_fm_unit_test
    unit/frontend/machine_audio_mixer_fm_unit_test.cpp
)
target_link_libraries(frontend_machine_audio_mixer_fm_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_audio_mixer_fm_unit_test
    COMMAND frontend_machine_audio_mixer_fm_unit_test)

# M37 Slice B (DEC-0055): inserted FM-PAC cartridge OPLL mixed additively.
add_executable(frontend_machine_audio_mixer_fmpac_unit_test
    unit/frontend/machine_audio_mixer_fmpac_unit_test.cpp
)
target_link_libraries(frontend_machine_audio_mixer_fmpac_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_audio_mixer_fmpac_unit_test
    COMMAND frontend_machine_audio_mixer_fmpac_unit_test)

add_executable(machine_hbf1xv_fm_activity_probe_integration_test
    integration/machine/hbf1xv_fm_activity_probe_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_fm_activity_probe_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_fm_activity_probe_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_fm_activity_probe_integration_test
    COMMAND machine_hbf1xv_fm_activity_probe_integration_test)

add_executable(machine_hbf1xv_scc_cartridge_integration_test
    integration/machine/hbf1xv_scc_cartridge_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_scc_cartridge_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_scc_cartridge_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}"
    SONY_MSX_SOFTWAREDB_PATH="${SONY_MSX_SOFTWAREDB_PATH_ABS}")
add_test(NAME machine_hbf1xv_scc_cartridge_integration_test
    COMMAND machine_hbf1xv_scc_cartridge_integration_test)

add_executable(fm_synth_system_test
    system/fm_synth_system_test.cpp
)
target_link_libraries(fm_synth_system_test PRIVATE sony_msx_core)
add_test(NAME fm_synth_system_test COMMAND fm_synth_system_test)

# --- M28-S1 YM2413 register-write timing constraint (backlog E2): E2 gate
#     unit tests (FakeClock, 12/84-cycle boundaries, determinism) + a machine-
#     level integration test driving real CPU OUT (#7C)/OUT (#7D) sequences
#     over the M11 bus at varying spacing (docs/m28-planner-package.md §2.1b).
add_executable(devices_audio_ym2413_write_timing_unit_test
    unit/devices/audio_ym2413_write_timing_unit_test.cpp
)
target_link_libraries(devices_audio_ym2413_write_timing_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_ym2413_write_timing_unit_test
    COMMAND devices_audio_ym2413_write_timing_unit_test)

add_executable(machine_hbf1xv_ym2413_write_timing_integration_test
    integration/machine/hbf1xv_ym2413_write_timing_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_ym2413_write_timing_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_ym2413_write_timing_integration_test
    COMMAND machine_hbf1xv_ym2413_write_timing_integration_test)

# --- M28-S2 C5 full-boot / auto-disk-boot-trigger investigation: cold-boots
#     the real BIOS with a REAL MSX-DOS system disk (disks/msxdos22.dsk)
#     mounted, with and without a scripted keypress sequence, tracking
#     whether the disk-ROM window is ever paged into page 1 at all
#     (docs/m28-planner-package.md §2.1a).
add_executable(disk_boot_investigation_system_test
    system/disk_boot_investigation_system_test.cpp
)
target_link_libraries(disk_boot_investigation_system_test PRIVATE sony_msx_core)
target_compile_definitions(disk_boot_investigation_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_DISKS_DIR="${CMAKE_SOURCE_DIR}/disks")
add_test(NAME disk_boot_investigation_system_test
    COMMAND disk_boot_investigation_system_test)

# --- Border-box + boot-logo targeted defect cycle (DEC-0026/DEC-0029
#     precedent; docs/border-and-boot-logo-investigation.md): the #F4
#     reset-status latch, line-granular S#0 collision re-latching, the
#     pre-armed R#44 transfer unit, the GRAPHIC5/6 sprite-compositor edge
#     clip, the presentation border-canvas composition, and the end-to-end
#     real-BIOS boot-logo sequence guard. All headless-buildable.
add_executable(devices_chipset_reset_status_register_unit_test
    unit/devices/chipset_reset_status_register_unit_test.cpp
)
target_link_libraries(devices_chipset_reset_status_register_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_chipset_reset_status_register_unit_test
    COMMAND devices_chipset_reset_status_register_unit_test)

add_executable(devices_sprite_engine_collision_relatch_unit_test
    unit/devices/video_sprite_engine_collision_relatch_unit_test.cpp
)
target_link_libraries(devices_sprite_engine_collision_relatch_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_sprite_engine_collision_relatch_unit_test
    COMMAND devices_sprite_engine_collision_relatch_unit_test)

add_executable(devices_vdp_command_engine_pending_col_unit_test
    unit/devices/video_vdp_command_engine_pending_col_unit_test.cpp
)
target_link_libraries(devices_vdp_command_engine_pending_col_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_command_engine_pending_col_unit_test
    COMMAND devices_vdp_command_engine_pending_col_unit_test)

add_executable(devices_vdp_frame_renderer_sprite_edge_clip_unit_test
    unit/devices/video_vdp_frame_renderer_sprite_edge_clip_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_sprite_edge_clip_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_sprite_edge_clip_unit_test
    COMMAND devices_vdp_frame_renderer_sprite_edge_clip_unit_test)

add_executable(devices_vdp_frame_renderer_color0_backdrop_unit_test
    unit/devices/video_vdp_frame_renderer_color0_backdrop_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_color0_backdrop_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_color0_backdrop_unit_test
    COMMAND devices_vdp_frame_renderer_color0_backdrop_unit_test)

add_executable(frontend_border_composer_unit_test
    unit/frontend/border_composer_unit_test.cpp
)
target_link_libraries(frontend_border_composer_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_border_composer_unit_test COMMAND frontend_border_composer_unit_test)

# Phosphor-persistence inter-frame blend (frontend/phosphor_blend.h) pure math.
# Header-only + SDL3-free, so registered OUTSIDE the SONY_MSX_ENABLE_SDL3 guard
# (mirrors border_composer_unit_test / sdl3_cli_unit_test placement).
add_executable(frontend_phosphor_blend_unit_test
    unit/frontend/phosphor_blend_unit_test.cpp
)
target_link_libraries(frontend_phosphor_blend_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_phosphor_blend_unit_test COMMAND frontend_phosphor_blend_unit_test)

add_executable(machine_hbf1xv_boot_logo_integration_test
    integration/machine/hbf1xv_boot_logo_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_boot_logo_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_boot_logo_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_boot_logo_integration_test
    COMMAND machine_hbf1xv_boot_logo_integration_test)

# --- M32 raster-accurate per-line rendering + line-interrupt delivery +
#     FM mix calibration (DEC-0039 defect pair, docs/m32-planner-package.md):
#     the VdpScanlineAccumulator static-frame-equivalence hard oracle (AC-4),
#     the V9958 render-sync seam contract, the machine-level L+1 latch rule
#     (incl. the debug_io_write hook-exclusion property), the line-interrupt
#     trigger rule ((R#19 - R#23) & 0xFF, both-references-agreed), the
#     synthetic split-screen system oracle with its adversarial IE1-off arm,
#     and the FM loudness-ratio oracle. All headless-buildable. -----------
add_executable(devices_vdp_scanline_accumulator_unit_test
    unit/devices/video_vdp_scanline_accumulator_unit_test.cpp
)
target_link_libraries(devices_vdp_scanline_accumulator_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_scanline_accumulator_unit_test
    COMMAND devices_vdp_scanline_accumulator_unit_test)

add_executable(devices_v9958_render_sync_seam_unit_test
    unit/devices/video_v9958_render_sync_seam_unit_test.cpp
)
target_link_libraries(devices_v9958_render_sync_seam_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_render_sync_seam_unit_test
    COMMAND devices_v9958_render_sync_seam_unit_test)

add_executable(machine_hbf1xv_per_line_latch_integration_test
    integration/machine/hbf1xv_per_line_latch_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_per_line_latch_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_per_line_latch_integration_test
    COMMAND machine_hbf1xv_per_line_latch_integration_test)

add_executable(machine_hbf1xv_line_interrupt_integration_test
    integration/machine/hbf1xv_line_interrupt_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_line_interrupt_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_line_interrupt_integration_test
    COMMAND machine_hbf1xv_line_interrupt_integration_test)

add_executable(split_screen_system_test
    system/split_screen_system_test.cpp
)
target_link_libraries(split_screen_system_test PRIVATE sony_msx_core)
add_test(NAME split_screen_system_test
    COMMAND split_screen_system_test)

# M49-S2 (backlog D9, DEC-0076): the sprite-split-coincidence oracle -- the
# incremental per-line-live sprite plane splits at the SAME display line as the
# background split (sprites ENABLED, driven through the real render-sync seam).
add_executable(sprite_split_coincidence_system_test
    system/sprite_split_coincidence_system_test.cpp
)
target_link_libraries(sprite_split_coincidence_system_test PRIVATE sony_msx_core)
add_test(NAME sprite_split_coincidence_system_test
    COMMAND sprite_split_coincidence_system_test)

# M44 (DEF-M44-CMDSYNC Phase 1, DEC-0065): command-engine per-destination-row
# render-sync -- per-row observation-ordering oracle + non-tautology + inertness.
add_executable(machine_hbf1xv_command_render_sync_integration_test
    integration/machine/hbf1xv_command_render_sync_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_command_render_sync_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_command_render_sync_integration_test
    COMMAND machine_hbf1xv_command_render_sync_integration_test)

add_executable(frontend_machine_audio_mixer_loudness_ratio_unit_test
    unit/frontend/machine_audio_mixer_loudness_ratio_unit_test.cpp
)
target_link_libraries(frontend_machine_audio_mixer_loudness_ratio_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_audio_mixer_loudness_ratio_unit_test
    COMMAND frontend_machine_audio_mixer_loudness_ratio_unit_test)

# --- M34-S3 Defect-A hard oracles (DEC-0043, docs/m34-planner-package.md
#     §2.6): the ultrasonic-silence family (exact §2.4 dwell bounds p=0..4 +
#     the 1 kHz control arm + SCC fixed-point arm), the mid-band fidelity
#     oracle (exact 12,400 peak / transition confinement / RMS vs the
#     ideal-square predictor), and the scroll-shooter transition regression (the
#     recorded m32 smoke recipe, frames 2150-2350, ZCR burst metric --
#     asset-gated skip per the DEC-0016 discipline). ---
add_executable(frontend_machine_audio_mixer_ultrasonic_silence_unit_test
    unit/frontend/machine_audio_mixer_ultrasonic_silence_unit_test.cpp
)
target_link_libraries(frontend_machine_audio_mixer_ultrasonic_silence_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_audio_mixer_ultrasonic_silence_unit_test
    COMMAND frontend_machine_audio_mixer_ultrasonic_silence_unit_test)

add_executable(frontend_machine_audio_mixer_midband_fidelity_unit_test
    unit/frontend/machine_audio_mixer_midband_fidelity_unit_test.cpp
)
target_link_libraries(frontend_machine_audio_mixer_midband_fidelity_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_machine_audio_mixer_midband_fidelity_unit_test
    COMMAND frontend_machine_audio_mixer_midband_fidelity_unit_test)

add_executable(ultrasonic_regression_system_test
    system/ultrasonic_regression_system_test.cpp
)
target_link_libraries(ultrasonic_regression_system_test PRIVATE sony_msx_core)
target_compile_definitions(ultrasonic_regression_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME ultrasonic_regression_system_test
    COMMAND ultrasonic_regression_system_test)

# --- M34-S5 Defect-B hard oracles (DEC-0043, docs/m34-planner-package.md
#     §3.4): the R#1 BL render-gate unit oracle (per-mode pure backdrop +
#     sprites-absent + BL=1 content) and the machine-level L+1 BL latch
#     through the REAL seam (+ debug-seam exclusion + determinism). ---
add_executable(devices_vdp_frame_renderer_bl_unit_test
    unit/devices/video_vdp_frame_renderer_bl_unit_test.cpp
)
target_link_libraries(devices_vdp_frame_renderer_bl_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_vdp_frame_renderer_bl_unit_test
    COMMAND devices_vdp_frame_renderer_bl_unit_test)

add_executable(machine_hbf1xv_bl_latch_integration_test
    integration/machine/hbf1xv_bl_latch_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_bl_latch_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_bl_latch_integration_test
    COMMAND machine_hbf1xv_bl_latch_integration_test)

# --- M36 Phase 3 comprehensive debug snapshot (DEC-0051, docs/m36-phase3-
#     planner-package.md). Per-slice typed-field exactness (S1 core/memory/
#     clock/slots + golden-unchanged + no-op guard, S2 VDP, S3 audio, S4
#     FDC/RTC/chipset/cartridge), the parse-regression guard, and the S5
#     adversarial cross-run determinism system test. ------------------------
add_executable(machine_debug_snapshot_core_unit_test
    unit/machine/debug_snapshot_core_unit_test.cpp
)
target_link_libraries(machine_debug_snapshot_core_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_debug_snapshot_core_unit_test COMMAND machine_debug_snapshot_core_unit_test)

add_executable(machine_debug_snapshot_vdp_unit_test
    unit/machine/debug_snapshot_vdp_unit_test.cpp
)
target_link_libraries(machine_debug_snapshot_vdp_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_debug_snapshot_vdp_unit_test COMMAND machine_debug_snapshot_vdp_unit_test)

add_executable(machine_debug_snapshot_audio_unit_test
    unit/machine/debug_snapshot_audio_unit_test.cpp
)
target_link_libraries(machine_debug_snapshot_audio_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_debug_snapshot_audio_unit_test COMMAND machine_debug_snapshot_audio_unit_test)

add_executable(machine_debug_snapshot_devices_unit_test
    unit/machine/debug_snapshot_devices_unit_test.cpp
)
target_link_libraries(machine_debug_snapshot_devices_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_debug_snapshot_devices_unit_test COMMAND machine_debug_snapshot_devices_unit_test)

add_executable(frontend_sdl3_cli_snapshot_unit_test
    unit/frontend/sdl3_cli_snapshot_unit_test.cpp
)
target_link_libraries(frontend_sdl3_cli_snapshot_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_sdl3_cli_snapshot_unit_test COMMAND frontend_sdl3_cli_snapshot_unit_test)

add_executable(snapshot_determinism_system_test
    system/snapshot_determinism_system_test.cpp
)
target_link_libraries(snapshot_determinism_system_test PRIVATE sony_msx_core)
target_compile_definitions(snapshot_determinism_system_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME snapshot_determinism_system_test
    COMMAND snapshot_determinism_system_test)

# --- DEC-0052 live stream-capture (F10): per-frame bundles + auto-stop-on-HALT
#     trace ring, end-to-end against real on-disk output + determinism oracle. ---
add_executable(stream_capture_system_test
    system/stream_capture_system_test.cpp
)
target_link_libraries(stream_capture_system_test PRIVATE sony_msx_core)
add_test(NAME stream_capture_system_test
    COMMAND stream_capture_system_test)

# --- M26-S2..S8: SDL3-dependent tests (compiled/registered ONLY under
#     -DSONY_MSX_ENABLE_SDL3=ON, docs/m26-planner-package.md §2.1/§3). The
#     default headless (-DSONY_MSX_ENABLE_SDL3=OFF) ctest suite is completely
#     unaffected by this block -- confirmed by construction (CMake never
#     evaluates it when the option is OFF). Every test here runs entirely
#     under SDL_VIDEO_DRIVER=dummy/SDL_AUDIO_DRIVER=dummy (A-M26-2), with NO
#     real display/audio hardware required, and NO SDL_Delay-based pacing
#     anywhere in the ctest execution path (A-M26-8) -- every test calls
#     Sdl3App::run_one_frame() directly, a bounded number of times, never
#     run_interactive().
if(SONY_MSX_ENABLE_SDL3)
    add_executable(frontend_sdl3_app_headless_integration_test
        integration/frontend/sdl3_app_headless_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_headless_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_headless_integration_test
        PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_headless_integration_test
        COMMAND frontend_sdl3_app_headless_integration_test)

    add_executable(frontend_sdl3_video_presenter_pixel_integration_test
        integration/frontend/sdl3_video_presenter_pixel_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_video_presenter_pixel_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_video_presenter_pixel_integration_test
        COMMAND frontend_sdl3_video_presenter_pixel_integration_test)

    add_executable(frontend_sdl3_audio_presenter_integration_test
        integration/frontend/sdl3_audio_presenter_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_audio_presenter_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_audio_presenter_integration_test
        COMMAND frontend_sdl3_audio_presenter_integration_test)

    # M57 (DEC-0085, §3.6): the PERMANENT audio-emission oracle -- drives a REAL
    # Sdl3App through run_one_frame() under dummy audio + hidden-window and asserts
    # REAL-sample PCM bytes reach SDL_PutAudioStreamData (non-tautological: counts
    # only real, non-silence-prime bytes). Closes the "green but silent" blind spot.
    add_executable(frontend_sdl3_audio_emission_integration_test
        integration/frontend/sdl3_audio_emission_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_audio_emission_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_audio_emission_integration_test
        COMMAND frontend_sdl3_audio_emission_integration_test)

    add_executable(frontend_sdl3_input_mapper_integration_test
        integration/frontend/sdl3_input_mapper_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_input_mapper_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_input_mapper_integration_test
        COMMAND frontend_sdl3_input_mapper_integration_test)

    # --- M35-S3: F11 hotkey input mapper unit test (multi-disk hot-swap,
    #     docs/m35-planner-package.md §5, UT-2). SDL3-gated (requires
    #     sdl3_input_mapper.h which depends on SDL3/SDL.h).
    add_executable(frontend_sdl3_input_mapper_disk_swap_unit_test
        unit/frontend/sdl3_input_mapper_disk_swap_unit_test.cpp
    )
    target_link_libraries(frontend_sdl3_input_mapper_disk_swap_unit_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_input_mapper_disk_swap_unit_test
        COMMAND frontend_sdl3_input_mapper_disk_swap_unit_test)

    set(SONY_MSX_DISKS_DIR_ABS "${CMAKE_SOURCE_DIR}/disks")

    add_executable(frontend_sdl3_cli_session_integration_test
        integration/frontend/sdl3_cli_session_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_cli_session_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_cli_session_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}"
        SONY_MSX_DISKS_DIR="${SONY_MSX_DISKS_DIR_ABS}"
        # M30 (backlog G2): the additive auto-identification Case 3
        # (skip-gated on ROM presence + exact SHA1 + DB presence).
        SONY_MSX_SOFTWAREDB_PATH="${SONY_MSX_SOFTWAREDB_PATH_ABS}")
    add_test(NAME frontend_sdl3_cli_session_integration_test
        COMMAND frontend_sdl3_cli_session_integration_test)

    add_executable(sdl3_system_test
        system/sdl3_system_test.cpp
    )
    target_link_libraries(sdl3_system_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(sdl3_system_test
        PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME sdl3_system_test COMMAND sdl3_system_test)

    # --- M27-S4/S7: SDL3-side debug-session/scripted-input wiring +
    #     cross-consistency/cross-executable-equivalence tests (docs/m27-
    #     planner-package.md §2.2/§2.4/§3). ---------------------------------
    add_executable(frontend_sdl3_app_debug_session_integration_test
        integration/frontend/sdl3_app_debug_session_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_debug_session_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_app_debug_session_integration_test
        COMMAND frontend_sdl3_app_debug_session_integration_test)

    add_executable(frontend_sdl3_input_mapper_key_names_consistency_integration_test
        integration/frontend/sdl3_input_mapper_key_names_consistency_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_input_mapper_key_names_consistency_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_input_mapper_key_names_consistency_integration_test
        COMMAND frontend_sdl3_input_mapper_key_names_consistency_integration_test)

    add_executable(frontend_sdl3_app_input_script_integration_test
        integration/frontend/sdl3_app_input_script_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_input_script_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_app_input_script_integration_test
        COMMAND frontend_sdl3_app_input_script_integration_test)

    # Input RECORDER (DEC-0072): end-to-end --record-input through the real
    # Sdl3App poll path (SDL_PushEvent -> recorder file), F11-swap comment
    # capture, then a record->replay round-trip back through --input-script.
    add_executable(frontend_sdl3_app_record_input_integration_test
        integration/frontend/sdl3_app_record_input_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_record_input_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_record_input_integration_test
        PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_record_input_integration_test
        COMMAND frontend_sdl3_app_record_input_integration_test)

    # --- M35-S2/S4/S5: Multi-disk hot-swap integration tests (docs/m35-
    #     planner-package.md §5, IT-1..IT-6). Tests boot with repeatable
    #     --disk lists, verify disk attachment, and check regression guards.
    add_executable(frontend_sdl3_app_multi_disk_integration_test
        integration/frontend/sdl3_app_multi_disk_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_multi_disk_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_multi_disk_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_DISKS_DIR="${SONY_MSX_DISKS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_multi_disk_integration_test
        COMMAND frontend_sdl3_app_multi_disk_integration_test)

    # --- M36 Phase 3: F12 snapshot path via the Sdl3App (DEC-0051, slice S5). ---
    add_executable(frontend_sdl3_app_snapshot_integration_test
        integration/frontend/sdl3_app_snapshot_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_snapshot_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_snapshot_integration_test
        PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_snapshot_integration_test
        COMMAND frontend_sdl3_app_snapshot_integration_test)

    # --- M36: FM-PAC battery-SRAM persistence via the Sdl3App (auto-derived
    #     <cart>.rom.sram default + --fmpac-sram/--no-fmpac-sram). Proves the
    #     SDL3 build persists FM-PAC saves across quit/reload (round-trip). ---
    add_executable(frontend_sdl3_app_fmpac_sram_integration_test
        integration/frontend/sdl3_app_fmpac_sram_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_fmpac_sram_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_fmpac_sram_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_fmpac_sram_integration_test
        COMMAND frontend_sdl3_app_fmpac_sram_integration_test)

    # --- M46 (DEC-0071): FM-PAC slot-2 auto-load wiring via the Sdl3App
    #     (auto-load + SRAM bind + coexist-with-slot-1 + explicit-slot2-wins +
    #     graceful absent/corrupt skip + no-double-FM-PAC). Uses a temp copy of
    #     the FM-PAC ROM so a real roms/fmpac.rom.sram save is never touched. ---
    add_executable(frontend_sdl3_app_fmpac_autoload_integration_test
        integration/frontend/sdl3_app_fmpac_autoload_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_fmpac_autoload_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_fmpac_autoload_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_fmpac_autoload_integration_test
        COMMAND frontend_sdl3_app_fmpac_autoload_integration_test)

    # --- M37 Slices D + E (DEC-0056): --speed initial Speed Controller level +
    #     resizable window / 320x240 letterbox logical presentation / texture
    #     filter threaded through Sdl3App::init(). ---
    add_executable(frontend_sdl3_app_scaling_integration_test
        integration/frontend/sdl3_app_scaling_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_scaling_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_app_scaling_integration_test
        COMMAND frontend_sdl3_app_scaling_integration_test)

    # --- M55 (DEC-0083): the ImGui menu determinism guard. A --hidden-window
    #     launch must create NO ImGui context (menu_active() == false) and still
    #     advance state by the pre-M55 oracle. Links the SDL3 frontend lib (which
    #     now links imgui), so it is SDL3-gated. ---
    add_executable(frontend_sdl3_menu_gate_integration_test
        integration/frontend/sdl3_menu_gate_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_menu_gate_integration_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_menu_gate_integration_test
        PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME frontend_sdl3_menu_gate_integration_test
        COMMAND frontend_sdl3_menu_gate_integration_test)

    # --- M56 (DEC-0084), slice S1: the reset-determinism oracle for
    #     Sdl3App::request_reset() (F4 Reset). Oracle A (post-reset == post-init) +
    #     Oracle B (fresh-boot + K == reset + K), bare config; plus disk/cart
    #     survive-reset (skip-gated on the untracked disks/roms assets). ---
    add_executable(frontend_sdl3_app_reset_determinism_integration_test
        integration/frontend/sdl3_app_reset_determinism_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_reset_determinism_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_reset_determinism_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_DISKS_DIR="${SONY_MSX_DISKS_DIR_ABS}"
        SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_reset_determinism_integration_test
        COMMAND frontend_sdl3_app_reset_determinism_integration_test)

    # --- M57 (DEC-0085-AMENDMENT-A): the power-cycle / live RAM-change oracle
    #     (fresh-boot equivalence + #FC segment-walk size proof + disk survival +
    #     transactional abort + hidden-window gate). Self-contained temp media. ---
    add_executable(frontend_sdl3_app_power_cycle_integration_test
        integration/frontend/sdl3_app_power_cycle_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_power_cycle_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_power_cycle_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_power_cycle_integration_test
        COMMAND frontend_sdl3_app_power_cycle_integration_test)

    # --- M60 (DEC-0089): the Machine>BIOS Folder transactional-apply oracle
    #     (apply_bios_folder): abort on empty/partial/zero-byte/nonexistent
    #     folders with config + machine byte-identical; success = power-cycle
    #     into a full copied BIOS set (fresh-boot equivalence, asset_root moved);
    #     hidden-window dialog launcher inert. Temp folder copies of the tracked
    #     bios/ set -- self-contained, no untracked assets. ---
    add_executable(frontend_sdl3_app_bios_folder_integration_test
        integration/frontend/sdl3_app_bios_folder_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_bios_folder_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_bios_folder_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_bios_folder_integration_test
        COMMAND frontend_sdl3_app_bios_folder_integration_test)

    # --- M56 (DEC-0084), slice S2: F1 Open Disk(s) multi-select apply seam
    #     (apply_open_disks) -- REPLACE semantics, dirty-flush-before-replace,
    #     transactional abort, cancel/empty no-op, media change. ---
    add_executable(frontend_sdl3_app_open_disks_integration_test
        integration/frontend/sdl3_app_open_disks_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_open_disks_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_open_disks_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_DISKS_DIR="${SONY_MSX_DISKS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_open_disks_integration_test
        COMMAND frontend_sdl3_app_open_disks_integration_test)

    # --- M56 (DEC-0084), slice S3: F2 Open Cartridge insert (apply_open_cartridge)
    #     + F3 Eject matrix (eject_disk / eject_cartridge). Skip-gated on the
    #     untracked FM-PAC ROM asset. ---
    add_executable(frontend_sdl3_app_cartridge_insert_integration_test
        integration/frontend/sdl3_app_cartridge_insert_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_cartridge_insert_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_cartridge_insert_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}"
        SONY_MSX_DISKS_DIR="${SONY_MSX_DISKS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_cartridge_insert_integration_test
        COMMAND frontend_sdl3_app_cartridge_insert_integration_test)

    add_executable(frontend_sdl3_app_eject_integration_test
        integration/frontend/sdl3_app_eject_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_eject_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_eject_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
        SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}"
        SONY_MSX_DISKS_DIR="${SONY_MSX_DISKS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_eject_integration_test
        COMMAND frontend_sdl3_app_eject_integration_test)

    # --- M56 (DEC-0084), slice S2/S4 (R6): the hidden-window determinism gate --
    #     the M56 dialog/reset machinery never fires headless (no mailbox pending,
    #     no apply_* ever runs, frame/cycle advance matches the headless oracle). ---
    add_executable(frontend_sdl3_app_hidden_window_gate_integration_test
        integration/frontend/sdl3_app_hidden_window_gate_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_hidden_window_gate_integration_test
        PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(frontend_sdl3_app_hidden_window_gate_integration_test PRIVATE
        SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME frontend_sdl3_app_hidden_window_gate_integration_test
        COMMAND frontend_sdl3_app_hidden_window_gate_integration_test)

    # M33/DEC-0041: stage SDL3.dll beside every SDL3-linked test executable
    # (see the sony_msx_stage_sdl3_dll definition in the root CMakeLists.txt --
    # without this, each test dies 0xc0000135 with a MODAL loader dialog that
    # blocks unattended ctest). Any future SDL3-linked test target must be
    # added to this list.
    foreach(sdl3_test_target
        frontend_sdl3_app_headless_integration_test
        frontend_sdl3_video_presenter_pixel_integration_test
        frontend_sdl3_audio_presenter_integration_test
        frontend_sdl3_input_mapper_integration_test
        frontend_sdl3_input_mapper_disk_swap_unit_test
        frontend_sdl3_cli_session_integration_test
        sdl3_system_test
        frontend_sdl3_app_debug_session_integration_test
        frontend_sdl3_input_mapper_key_names_consistency_integration_test
        frontend_sdl3_app_input_script_integration_test
        frontend_sdl3_app_record_input_integration_test
        frontend_sdl3_app_multi_disk_integration_test
        frontend_sdl3_app_snapshot_integration_test
        frontend_sdl3_app_fmpac_sram_integration_test
        frontend_sdl3_app_fmpac_autoload_integration_test
        frontend_sdl3_app_scaling_integration_test
        frontend_sdl3_menu_gate_integration_test
        frontend_sdl3_app_reset_determinism_integration_test
        frontend_sdl3_app_open_disks_integration_test
        frontend_sdl3_app_cartridge_insert_integration_test
        frontend_sdl3_app_eject_integration_test
        frontend_sdl3_app_hidden_window_gate_integration_test)
        sony_msx_stage_sdl3_dll(${sdl3_test_target})
    endforeach()
endif()

# --- M51 (DEC-0078): command-row sprite pacing -------------------------------
# The consumer-side sprite-pacing contract (fix branch (a) shape (i)):
# V9958Vdp::commit_sprite_rows() semantics at the device level, and the
# machine-level command-sink scenario reproducing the scroll-shooter /
# sprite-scroll / split-screen-title sprite-disappearance mechanism (H1)
# pre-fix and locking the fix.
add_executable(devices_v9958_command_row_sprite_pacing_unit_test
    unit/devices/video_v9958_command_row_sprite_pacing_unit_test.cpp
)
target_link_libraries(devices_v9958_command_row_sprite_pacing_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_v9958_command_row_sprite_pacing_unit_test
    COMMAND devices_v9958_command_row_sprite_pacing_unit_test)

add_executable(machine_hbf1xv_command_sprite_pacing_integration_test
    integration/machine/hbf1xv_command_sprite_pacing_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_command_sprite_pacing_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_command_sprite_pacing_integration_test
    COMMAND machine_hbf1xv_command_sprite_pacing_integration_test)

# --- M53 (DEC-0080): standalone MSX disk utility (msx-disk) ------------------
# 5 unit + 1 integration. The unit tests link the ISOLATED msx_diskutil static
# lib. Two cross-oracle tests (BPB-vs-machine, FDC-mount) additionally link
# sony_msx_core -- allowed for TESTS only; the shipped msx-disk binary never
# links the core (one-way build isolation, planner package §3).
add_executable(diskutils_blank_layout_unit_test
    unit/diskutils/diskutils_blank_layout_unit_test.cpp
)
target_link_libraries(diskutils_blank_layout_unit_test PRIVATE msx_diskutil)
add_test(NAME diskutils_blank_layout_unit_test COMMAND diskutils_blank_layout_unit_test)

add_executable(diskutils_bpb_matches_machine_unit_test
    unit/diskutils/diskutils_bpb_matches_machine_unit_test.cpp
)
target_link_libraries(diskutils_bpb_matches_machine_unit_test PRIVATE msx_diskutil sony_msx_core)
target_compile_definitions(diskutils_bpb_matches_machine_unit_test
    PRIVATE SONY_MSX_DISKS_DIR="${CMAKE_SOURCE_DIR}/disks")
add_test(NAME diskutils_bpb_matches_machine_unit_test COMMAND diskutils_bpb_matches_machine_unit_test)

add_executable(diskutils_create_format_roundtrip_unit_test
    unit/diskutils/diskutils_create_format_roundtrip_unit_test.cpp
)
target_link_libraries(diskutils_create_format_roundtrip_unit_test PRIVATE msx_diskutil)
add_test(NAME diskutils_create_format_roundtrip_unit_test
    COMMAND diskutils_create_format_roundtrip_unit_test)

add_executable(diskutils_hexdump_unit_test
    unit/diskutils/diskutils_hexdump_unit_test.cpp
)
target_link_libraries(diskutils_hexdump_unit_test PRIVATE msx_diskutil)
add_test(NAME diskutils_hexdump_unit_test COMMAND diskutils_hexdump_unit_test)

add_executable(diskutils_cli_validation_unit_test
    unit/diskutils/diskutils_cli_validation_unit_test.cpp
)
target_link_libraries(diskutils_cli_validation_unit_test PRIVATE msx_diskutil)
add_test(NAME diskutils_cli_validation_unit_test COMMAND diskutils_cli_validation_unit_test)

add_executable(machine_diskutil_fdc_mount_integration_test
    integration/machine/machine_diskutil_fdc_mount_integration_test.cpp
)
target_link_libraries(machine_diskutil_fdc_mount_integration_test PRIVATE sony_msx_core msx_diskutil)
add_test(NAME machine_diskutil_fdc_mount_integration_test
    COMMAND machine_diskutil_fdc_mount_integration_test)

# --- M58 S8 (DEF-M58-DSKCHG) permanent display oracle: the pinned multi-disk
#     strategy title's CTRL-boot + disk-2-swap repro must render the coherent
#     GRAPHIC6 campaign map
#     at frame 14000, not colored noise (structural coherence metrics measured
#     against the real corrupt/clean captures in debug/m58/s8/). Guards the
#     drive-select gating of the /DSKCHG sense (sony_fdc.cpp): the unselected
#     0x7FFD read must not consume drive A's disk-changed one-shot. Game disks
#     are owner-provisioned untracked assets -> the test skips when absent.
add_executable(strategy_map_oracle_system_test
    system/strategy_map_oracle_system_test.cpp
)
target_link_libraries(strategy_map_oracle_system_test PRIVATE sony_msx_core)
target_compile_definitions(strategy_map_oracle_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_GAMES_DIR="${CMAKE_SOURCE_DIR}/games")
add_test(NAME strategy_map_oracle_system_test
    COMMAND strategy_map_oracle_system_test)

# --- M62 (DEC-0091-AMENDMENT-A): command-row-sink beam clamp -----------------
# The background-register half of the M51 sibling defect (the racing title's
# racing-view flash): the M44 command-row sink must never seal rows
# AHEAD of the render beam with commit-time register/SAT state -- rows ahead
# of the beam render per-line-live on the beam path. Machine-level mechanism
# oracle (register half + sprite half, fails pre-fix) ...
add_executable(machine_hbf1xv_command_beam_clamp_integration_test
    integration/machine/hbf1xv_command_beam_clamp_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_command_beam_clamp_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_command_beam_clamp_integration_test
    COMMAND machine_hbf1xv_command_beam_clamp_integration_test)

# ... plus the permanent F1 racing-window title oracle (the pinned M62 recipe:
# boot racing-d2, swap racing-d3 at f19000, race to f30000; 40 consecutive frames must
# show 0 anomalous dashboard frames / 0 transitions -- the openMSX reference
# rate; pre-fix HEAD fails at 3/40 on the 13-frame beat). Game disks are
# owner-provisioned untracked assets -> the test skips when absent.
add_executable(racing_stability_system_test
    system/racing_stability_system_test.cpp
)
target_link_libraries(racing_stability_system_test PRIVATE sony_msx_core)
target_compile_definitions(racing_stability_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_GAMES_DIR="${CMAKE_SOURCE_DIR}/games")
add_test(NAME racing_stability_system_test
    COMMAND racing_stability_system_test)
