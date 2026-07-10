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

add_executable(machine_hbf1xv_m13_mem_parity_checkpoint_integration_test
    integration/machine/hbf1xv_m13_mem_parity_checkpoint_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m13_mem_parity_checkpoint_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m13_mem_parity_checkpoint_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m13_mem_parity_checkpoint_integration_test
    COMMAND machine_hbf1xv_m13_mem_parity_checkpoint_integration_test)

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

add_executable(machine_hbf1xv_m11_parity_checkpoint_integration_test
    integration/machine/hbf1xv_m11_parity_checkpoint_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m11_parity_checkpoint_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m11_parity_checkpoint_integration_test COMMAND machine_hbf1xv_m11_parity_checkpoint_integration_test)

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

# M36 Bug B: IE0/IE1 register-write /INT re-evaluation (YS II crash fix).
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

add_executable(machine_hbf1xv_m15_devices_integration_test
    integration/machine/hbf1xv_m15_devices_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m15_devices_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m15_devices_integration_test
    COMMAND machine_hbf1xv_m15_devices_integration_test)

add_executable(machine_hbf1xv_m15_boot_checkpoint_integration_test
    integration/machine/hbf1xv_m15_boot_checkpoint_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m15_boot_checkpoint_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m15_boot_checkpoint_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m15_boot_checkpoint_integration_test
    COMMAND machine_hbf1xv_m15_boot_checkpoint_integration_test)

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

add_executable(machine_hbf1xv_m16_fdc_integration_test
    integration/machine/hbf1xv_m16_fdc_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m16_fdc_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m16_fdc_integration_test COMMAND machine_hbf1xv_m16_fdc_integration_test)

add_executable(machine_hbf1xv_m16_boot_checkpoint_integration_test
    integration/machine/hbf1xv_m16_boot_checkpoint_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m16_boot_checkpoint_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m16_boot_checkpoint_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m16_boot_checkpoint_integration_test
    COMMAND machine_hbf1xv_m16_boot_checkpoint_integration_test)

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

add_executable(machine_hbf1xv_m17_ym2413_integration_test
    integration/machine/hbf1xv_m17_ym2413_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m17_ym2413_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m17_ym2413_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m17_ym2413_integration_test COMMAND machine_hbf1xv_m17_ym2413_integration_test)

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

add_executable(machine_hbf1xv_m18_peripheral_io_integration_test
    integration/machine/hbf1xv_m18_peripheral_io_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m18_peripheral_io_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m18_peripheral_io_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m18_peripheral_io_integration_test
    COMMAND machine_hbf1xv_m18_peripheral_io_integration_test)

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

add_executable(machine_hbf1xv_m19_cartridge_integration_test
    integration/machine/hbf1xv_m19_cartridge_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m19_cartridge_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m19_cartridge_integration_test
    COMMAND machine_hbf1xv_m19_cartridge_integration_test)

set(SONY_MSX_ROMS_DIR_ABS "${CMAKE_SOURCE_DIR}/roms")

add_executable(machine_hbf1xv_m19_aleste_smoke_integration_test
    integration/machine/hbf1xv_m19_aleste_smoke_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m19_aleste_smoke_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m19_aleste_smoke_integration_test
    PRIVATE SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m19_aleste_smoke_integration_test
    COMMAND machine_hbf1xv_m19_aleste_smoke_integration_test)

add_executable(machine_hbf1xv_m19_metalgear_smoke_integration_test
    integration/machine/hbf1xv_m19_metalgear_smoke_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m19_metalgear_smoke_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m19_metalgear_smoke_integration_test
    PRIVATE SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m19_metalgear_smoke_integration_test
    COMMAND machine_hbf1xv_m19_metalgear_smoke_integration_test)

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

add_executable(machine_hbf1xv_m20_halnote_integration_test
    integration/machine/hbf1xv_m20_halnote_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m20_halnote_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m20_halnote_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m20_halnote_integration_test
    COMMAND machine_hbf1xv_m20_halnote_integration_test)

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

add_executable(machine_hbf1xv_m21_vdp_render_integration_test
    integration/machine/hbf1xv_m21_vdp_render_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m21_vdp_render_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m21_vdp_render_integration_test
    COMMAND machine_hbf1xv_m21_vdp_render_integration_test)

add_executable(hbf1xv_m21_vdp_render_system_test
    system/hbf1xv_m21_vdp_render_system_test.cpp
)
target_link_libraries(hbf1xv_m21_vdp_render_system_test PRIVATE sony_msx_core)
add_test(NAME hbf1xv_m21_vdp_render_system_test
    COMMAND hbf1xv_m21_vdp_render_system_test)

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
# Metal Gear sprite-invisibility fix, docs/sprite-invisibility-investigation.md).
add_executable(devices_sprite_engine_mode2_attribute_masking_unit_test
    unit/devices/video_sprite_engine_mode2_attribute_masking_unit_test.cpp
)
target_link_libraries(devices_sprite_engine_mode2_attribute_masking_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_sprite_engine_mode2_attribute_masking_unit_test
    COMMAND devices_sprite_engine_mode2_attribute_masking_unit_test)

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

add_executable(machine_hbf1xv_m22_sprites_integration_test
    integration/machine/hbf1xv_m22_sprites_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m22_sprites_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m22_sprites_integration_test
    COMMAND machine_hbf1xv_m22_sprites_integration_test)

add_executable(machine_hbf1xv_m22_command_engine_integration_test
    integration/machine/hbf1xv_m22_command_engine_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m22_command_engine_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m22_command_engine_integration_test
    COMMAND machine_hbf1xv_m22_command_engine_integration_test)

add_executable(hbf1xv_m22_sprites_command_engine_system_test
    system/hbf1xv_m22_sprites_command_engine_system_test.cpp
)
target_link_libraries(hbf1xv_m22_sprites_command_engine_system_test PRIVATE sony_msx_core)
add_test(NAME hbf1xv_m22_sprites_command_engine_system_test
    COMMAND hbf1xv_m22_sprites_command_engine_system_test)

# --- M23 Z80 HALT-R closure (C2, DONE) + VDP access-timing foundation --------
# (C1/D4, narrowed/non-gating) --------------------------------------------
add_executable(devices_z80a_halt_r_unit_test
    unit/devices/z80a_halt_r_unit_test.cpp
)
target_link_libraries(devices_z80a_halt_r_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_z80a_halt_r_unit_test COMMAND devices_z80a_halt_r_unit_test)

add_executable(machine_hbf1xv_m23_halt_r_integration_test
    integration/machine/hbf1xv_m23_halt_r_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m23_halt_r_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m23_halt_r_integration_test
    COMMAND machine_hbf1xv_m23_halt_r_integration_test)

add_executable(devices_video_vdp_access_timing_unit_test
    unit/devices/video_vdp_access_timing_unit_test.cpp
)
target_link_libraries(devices_video_vdp_access_timing_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_video_vdp_access_timing_unit_test
    COMMAND devices_video_vdp_access_timing_unit_test)

add_executable(machine_hbf1xv_m23_vdp_access_timing_non_gating_integration_test
    integration/machine/hbf1xv_m23_vdp_access_timing_non_gating_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m23_vdp_access_timing_non_gating_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m23_vdp_access_timing_non_gating_integration_test
    COMMAND machine_hbf1xv_m23_vdp_access_timing_non_gating_integration_test)

# --- M24 ZEXDOC/ZEXALL full parity sweep (backlog C3): generic CP/M/BDOS ----
#     harness (src/machine/cpm_bdos_harness.*) + the real GPL exerciser
#     system test. The harness's own unit tests use ONLY a hand-written
#     synthetic fixture (never the real binaries, planner package SS1.5/
#     Acceptance Criterion 2); the real references/zexall/*.com binaries are
#     loaded ONLY by the dedicated system test below. -----------------------
add_executable(machine_cpm_bdos_harness_unit_test
    unit/machine/cpm_bdos_harness_unit_test.cpp
)
target_link_libraries(machine_cpm_bdos_harness_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_cpm_bdos_harness_unit_test COMMAND machine_cpm_bdos_harness_unit_test)

add_executable(machine_hbf1xv_m24_cpm_run_integration_test
    integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m24_cpm_run_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m24_cpm_run_integration_test
    COMMAND machine_hbf1xv_m24_cpm_run_integration_test)

# Absolute path to the repo references/zexall/ directory (mirrors the
# established SONY_MSX_BIOS_DIR_ABS pattern above, per DEC-0016's standing
# watch-item: a test needing a real asset path MUST wire its own compile
# definition or it silently 0xFF-fills/degrades under ctest's differing
# working directory).
set(SONY_MSX_ZEXALL_DIR_ABS "${CMAKE_SOURCE_DIR}/references/zexall")

# NOTE (developer disclosure, M24-S3): this system test runs BOTH real GPL
# exerciser binaries (references/zexall/zexall.com, zexdoc.com) to genuine
# CP/M warm-boot completion, in-process, via CpmBdosHarness. Empirically
# measured wall-clock cost is on the order of tens of minutes total (see
# docs/m24-implementation-report.md for the actual measured figure) -- this
# materially lengthens a full `ctest` invocation. This is a disclosed,
# intentional consequence of Acceptance Criterion 3 (a genuine, non-truncated
# full-suite run), not an oversight; see the implementation report's runtime
# section for the full honest disclosure and the coordinator/QA escalation
# this milestone's planner package anticipated (R-M24-3).
add_executable(hbf1xv_m24_zexall_system_test
    system/hbf1xv_m24_zexall_system_test.cpp
)
target_link_libraries(hbf1xv_m24_zexall_system_test PRIVATE sony_msx_core)
target_compile_definitions(hbf1xv_m24_zexall_system_test
    PRIVATE SONY_MSX_ZEXALL_DIR="${SONY_MSX_ZEXALL_DIR_ABS}")
add_test(NAME hbf1xv_m24_zexall_system_test COMMAND hbf1xv_m24_zexall_system_test)
set_tests_properties(hbf1xv_m24_zexall_system_test PROPERTIES
    TIMEOUT 7200
    LABELS "m24_slow_full_sweep")

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

add_executable(machine_hbf1xv_m25_pause_integration_test
    integration/machine/hbf1xv_m25_pause_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m25_pause_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m25_pause_integration_test COMMAND machine_hbf1xv_m25_pause_integration_test)

add_executable(machine_hbf1xv_m25_speed_controller_integration_test
    integration/machine/hbf1xv_m25_speed_controller_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m25_speed_controller_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m25_speed_controller_integration_test
    COMMAND machine_hbf1xv_m25_speed_controller_integration_test)

add_executable(hbf1xv_m25_speed_pause_rensha_system_test
    system/hbf1xv_m25_speed_pause_rensha_system_test.cpp
)
target_link_libraries(hbf1xv_m25_speed_pause_rensha_system_test PRIVATE sony_msx_core)
add_test(NAME hbf1xv_m25_speed_pause_rensha_system_test
    COMMAND hbf1xv_m25_speed_pause_rensha_system_test)

# --- M26 SDL3 frontend (backlog C9): windowing/real-time loop, video/audio
#     presentation, input mapping, decoded-FrameBuffer PNG capture --------
# S1 (on_vsync_boundary() seam) and S4 (frame-dump capability) are headless-
# buildable and registered here, OUTSIDE the SONY_MSX_ENABLE_SDL3 guard below
# (docs/m26-planner-package.md §3, M26-S1/S4) -- the default headless ctest
# suite gains exactly these two new cases; every other M26 test requires a
# real SDL3 build and lives in the guarded block further down.
add_executable(machine_hbf1xv_m26_vsync_boundary_integration_test
    integration/machine/hbf1xv_m26_vsync_boundary_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m26_vsync_boundary_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m26_vsync_boundary_integration_test
    COMMAND machine_hbf1xv_m26_vsync_boundary_integration_test)

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

add_executable(frontend_psg_audio_dump_unit_test
    unit/frontend/psg_audio_dump_unit_test.cpp
)
target_link_libraries(frontend_psg_audio_dump_unit_test PRIVATE sony_msx_core)
add_test(NAME frontend_psg_audio_dump_unit_test COMMAND frontend_psg_audio_dump_unit_test)

add_executable(machine_hbf1xv_m27_debug_session_integration_test
    integration/machine/hbf1xv_m27_debug_session_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m27_debug_session_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m27_debug_session_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_DISKS_DIR="${CMAKE_SOURCE_DIR}/disks")
add_test(NAME machine_hbf1xv_m27_debug_session_integration_test
    COMMAND machine_hbf1xv_m27_debug_session_integration_test)

add_executable(machine_hbf1xv_m27_input_script_integration_test
    integration/machine/hbf1xv_m27_input_script_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m27_input_script_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m27_input_script_integration_test
    COMMAND machine_hbf1xv_m27_input_script_integration_test)

add_executable(hbf1xv_m27_replay_determinism_system_test
    system/hbf1xv_m27_replay_determinism_system_test.cpp
)
target_link_libraries(hbf1xv_m27_replay_determinism_system_test PRIVATE sony_msx_core)
target_compile_definitions(hbf1xv_m27_replay_determinism_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME hbf1xv_m27_replay_determinism_system_test COMMAND hbf1xv_m27_replay_determinism_system_test)

add_executable(hbf1xv_m27_debug_tooling_system_test
    system/hbf1xv_m27_debug_tooling_system_test.cpp
)
target_link_libraries(hbf1xv_m27_debug_tooling_system_test PRIVATE sony_msx_core)
target_compile_definitions(hbf1xv_m27_debug_tooling_system_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME hbf1xv_m27_debug_tooling_system_test COMMAND hbf1xv_m27_debug_tooling_system_test)

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
#     the new mixer-level ultrasonic/mid-band oracles and the Aleste
#     regression live further below); Defect B = the R#1 BL render gate
#     (renderer BL unit test + L+1 latch integration test below).
add_executable(devices_audio_dwell_rounding_unit_test
    unit/devices/audio_dwell_rounding_unit_test.cpp
)
target_link_libraries(devices_audio_dwell_rounding_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_audio_dwell_rounding_unit_test COMMAND devices_audio_dwell_rounding_unit_test)

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

add_executable(machine_hbf1xv_m29_konami_scc_integration_test
    integration/machine/hbf1xv_m29_konami_scc_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m29_konami_scc_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m29_konami_scc_integration_test
    COMMAND machine_hbf1xv_m29_konami_scc_integration_test)

# --- M36 (DEC-0050): FM-PAC peripheral cartridge + disk-save persistence -----
add_executable(devices_cartridge_fmpac_rom_unit_test
    unit/devices/cartridge/cartridge_fmpac_rom_unit_test.cpp
)
target_link_libraries(devices_cartridge_fmpac_rom_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_cartridge_fmpac_rom_unit_test COMMAND devices_cartridge_fmpac_rom_unit_test)

add_executable(devices_disk_image_writeback_unit_test
    unit/devices/fdc/disk_image_writeback_unit_test.cpp
)
target_link_libraries(devices_disk_image_writeback_unit_test PRIVATE sony_msx_core)
add_test(NAME devices_disk_image_writeback_unit_test COMMAND devices_disk_image_writeback_unit_test)

add_executable(machine_hbf1xv_m36_fmpac_cartridge_integration_test
    integration/machine/hbf1xv_m36_fmpac_cartridge_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m36_fmpac_cartridge_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m36_fmpac_cartridge_integration_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
            SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m36_fmpac_cartridge_integration_test
    COMMAND machine_hbf1xv_m36_fmpac_cartridge_integration_test)

add_executable(machine_hbf1xv_m36_disk_save_persist_integration_test
    integration/machine/hbf1xv_m36_disk_save_persist_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m36_disk_save_persist_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m36_disk_save_persist_integration_test
    COMMAND machine_hbf1xv_m36_disk_save_persist_integration_test)

add_executable(hbf1xv_m29_scc_system_test
    system/hbf1xv_m29_scc_system_test.cpp
)
target_link_libraries(hbf1xv_m29_scc_system_test PRIVATE sony_msx_core)
add_test(NAME hbf1xv_m29_scc_system_test COMMAND hbf1xv_m29_scc_system_test)

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

add_executable(machine_software_db_unit_test
    unit/machine/software_db_unit_test.cpp
)
target_link_libraries(machine_software_db_unit_test PRIVATE sony_msx_core)
add_test(NAME machine_software_db_unit_test COMMAND machine_software_db_unit_test)

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

add_executable(machine_hbf1xv_m30_cartridge_identification_integration_test
    integration/machine/hbf1xv_m30_cartridge_identification_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m30_cartridge_identification_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m30_cartridge_identification_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}"
    SONY_MSX_SOFTWAREDB_PATH="${SONY_MSX_SOFTWAREDB_PATH_ABS}")
add_test(NAME machine_hbf1xv_m30_cartridge_identification_integration_test
    COMMAND machine_hbf1xv_m30_cartridge_identification_integration_test)

# --- M31 YM2413 FM-synthesis DSP depth (backlog E1, docs/m31-planner-
#     package.md, RELEASE CANDIDATE): the formula-grounded Ym2413Synth unit
#     suites (S1 closed-form table oracles recomputed independently in-test;
#     S2 pitch/volume/feedback/half-sine oracles; S3 §5 decay/release exact
#     duration oracles + the disclosed attack approximation's qualitative
#     contract; S4 rhythm x2 gain law / keying / AM-VIB laws), the mixer
#     three-source suite (incl. the zero-YM2413 byte-identity HARD oracle
#     and the 9:8 decimation proof), the real-title FM probe + MG2 SCC
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

add_executable(machine_hbf1xv_m31_fm_title_probe_integration_test
    integration/machine/hbf1xv_m31_fm_title_probe_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m31_fm_title_probe_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m31_fm_title_probe_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME machine_hbf1xv_m31_fm_title_probe_integration_test
    COMMAND machine_hbf1xv_m31_fm_title_probe_integration_test)

add_executable(machine_hbf1xv_m31_metalgear2_scc_integration_test
    integration/machine/hbf1xv_m31_metalgear2_scc_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m31_metalgear2_scc_integration_test PRIVATE sony_msx_core)
target_compile_definitions(machine_hbf1xv_m31_metalgear2_scc_integration_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}"
    SONY_MSX_SOFTWAREDB_PATH="${SONY_MSX_SOFTWAREDB_PATH_ABS}")
add_test(NAME machine_hbf1xv_m31_metalgear2_scc_integration_test
    COMMAND machine_hbf1xv_m31_metalgear2_scc_integration_test)

add_executable(hbf1xv_m31_fm_synth_system_test
    system/hbf1xv_m31_fm_synth_system_test.cpp
)
target_link_libraries(hbf1xv_m31_fm_synth_system_test PRIVATE sony_msx_core)
add_test(NAME hbf1xv_m31_fm_synth_system_test COMMAND hbf1xv_m31_fm_synth_system_test)

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

add_executable(machine_hbf1xv_m28_ym2413_write_timing_integration_test
    integration/machine/hbf1xv_m28_ym2413_write_timing_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m28_ym2413_write_timing_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m28_ym2413_write_timing_integration_test
    COMMAND machine_hbf1xv_m28_ym2413_write_timing_integration_test)

# --- M28-S2 C5 full-boot / auto-disk-boot-trigger investigation: cold-boots
#     the real BIOS with a REAL MSX-DOS system disk (disks/msxdos22.dsk)
#     mounted, with and without a scripted keypress sequence, tracking
#     whether the disk-ROM window is ever paged into page 1 at all
#     (docs/m28-planner-package.md §2.1a).
add_executable(hbf1xv_m28_c5_disk_boot_investigation_system_test
    system/hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp
)
target_link_libraries(hbf1xv_m28_c5_disk_boot_investigation_system_test PRIVATE sony_msx_core)
target_compile_definitions(hbf1xv_m28_c5_disk_boot_investigation_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_DISKS_DIR="${CMAKE_SOURCE_DIR}/disks")
add_test(NAME hbf1xv_m28_c5_disk_boot_investigation_system_test
    COMMAND hbf1xv_m28_c5_disk_boot_investigation_system_test)

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

add_executable(machine_hbf1xv_m32_per_line_latch_integration_test
    integration/machine/hbf1xv_m32_per_line_latch_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m32_per_line_latch_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m32_per_line_latch_integration_test
    COMMAND machine_hbf1xv_m32_per_line_latch_integration_test)

add_executable(machine_hbf1xv_m32_line_interrupt_integration_test
    integration/machine/hbf1xv_m32_line_interrupt_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m32_line_interrupt_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m32_line_interrupt_integration_test
    COMMAND machine_hbf1xv_m32_line_interrupt_integration_test)

add_executable(hbf1xv_m32_split_screen_system_test
    system/hbf1xv_m32_split_screen_system_test.cpp
)
target_link_libraries(hbf1xv_m32_split_screen_system_test PRIVATE sony_msx_core)
add_test(NAME hbf1xv_m32_split_screen_system_test
    COMMAND hbf1xv_m32_split_screen_system_test)

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
#     ideal-square predictor), and the Aleste transition regression (the
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

add_executable(hbf1xv_m34_aleste_ultrasonic_regression_system_test
    system/hbf1xv_m34_aleste_ultrasonic_regression_system_test.cpp
)
target_link_libraries(hbf1xv_m34_aleste_ultrasonic_regression_system_test PRIVATE sony_msx_core)
target_compile_definitions(hbf1xv_m34_aleste_ultrasonic_regression_system_test PRIVATE
    SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}"
    SONY_MSX_ROMS_DIR="${SONY_MSX_ROMS_DIR_ABS}")
add_test(NAME hbf1xv_m34_aleste_ultrasonic_regression_system_test
    COMMAND hbf1xv_m34_aleste_ultrasonic_regression_system_test)

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

add_executable(machine_hbf1xv_m34_bl_latch_integration_test
    integration/machine/hbf1xv_m34_bl_latch_integration_test.cpp
)
target_link_libraries(machine_hbf1xv_m34_bl_latch_integration_test PRIVATE sony_msx_core)
add_test(NAME machine_hbf1xv_m34_bl_latch_integration_test
    COMMAND machine_hbf1xv_m34_bl_latch_integration_test)

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

add_executable(hbf1xv_m36_snapshot_determinism_system_test
    system/hbf1xv_m36_snapshot_determinism_system_test.cpp
)
target_link_libraries(hbf1xv_m36_snapshot_determinism_system_test PRIVATE sony_msx_core)
target_compile_definitions(hbf1xv_m36_snapshot_determinism_system_test
    PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
add_test(NAME hbf1xv_m36_snapshot_determinism_system_test
    COMMAND hbf1xv_m36_snapshot_determinism_system_test)

# --- DEC-0052 live stream-capture (F10): per-frame bundles + auto-stop-on-HALT
#     trace ring, end-to-end against real on-disk output + determinism oracle. ---
add_executable(hbf1xv_m36_stream_capture_system_test
    system/hbf1xv_m36_stream_capture_system_test.cpp
)
target_link_libraries(hbf1xv_m36_stream_capture_system_test PRIVATE sony_msx_core)
add_test(NAME hbf1xv_m36_stream_capture_system_test
    COMMAND hbf1xv_m36_stream_capture_system_test)

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

    add_executable(hbf1xv_m26_sdl3_system_test
        system/hbf1xv_m26_sdl3_system_test.cpp
    )
    target_link_libraries(hbf1xv_m26_sdl3_system_test PRIVATE sony_msx_sdl3_frontend)
    target_compile_definitions(hbf1xv_m26_sdl3_system_test
        PRIVATE SONY_MSX_BIOS_DIR="${SONY_MSX_BIOS_DIR_ABS}")
    add_test(NAME hbf1xv_m26_sdl3_system_test COMMAND hbf1xv_m26_sdl3_system_test)

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

    # --- M37 Slices D + E (DEC-0056): --speed initial Speed Controller level +
    #     resizable window / 320x240 letterbox logical presentation / texture
    #     filter threaded through Sdl3App::init(). ---
    add_executable(frontend_sdl3_app_scaling_integration_test
        integration/frontend/sdl3_app_scaling_integration_test.cpp
    )
    target_link_libraries(frontend_sdl3_app_scaling_integration_test PRIVATE sony_msx_sdl3_frontend)
    add_test(NAME frontend_sdl3_app_scaling_integration_test
        COMMAND frontend_sdl3_app_scaling_integration_test)

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
        hbf1xv_m26_sdl3_system_test
        frontend_sdl3_app_debug_session_integration_test
        frontend_sdl3_input_mapper_key_names_consistency_integration_test
        frontend_sdl3_app_input_script_integration_test
        frontend_sdl3_app_multi_disk_integration_test
        frontend_sdl3_app_snapshot_integration_test
        frontend_sdl3_app_fmpac_sram_integration_test
        frontend_sdl3_app_scaling_integration_test)
        sony_msx_stage_sdl3_dll(${sdl3_test_target})
    endforeach()
endif()
