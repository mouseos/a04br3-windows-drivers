CC ?= cc
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
SRC := drivers/storage/mt8167-msdc

.PHONY: test test-thermal test-ntfs-root arm-coff clean

test: build/gpu_render_pds_data_test build/gpu_windows_render_memory_test build/gpu_render_kick_off build/gpu_render_kick_on build/gpu_windows_render_command_test build/gpu_render_execution_test test-thermal build/bq24160_handoff_test build/bq24160_charger_test build/bq24160_i2c_test build/gpu_render_context_test build/gpu_render_hwrt_test build/gpu_render_freelist_test build/gpu_render_stream_test build/lc709203f_test build/lc709203f_i2c_test build/msdc_io_gate_test build/toprgu_watchdog_policy_test build/toprgu_reset_command_test build/cdb_test build/pio_test build/scsi_capacity_test build/dma_test build/scsi_test build/gpt_guard_test build/button_state_test build/button_pipeline_test build/button_device_test build/button_hardware_test build/pmic_read_test build/bu21025_test build/i2c_pio_test build/bu21025_bus_test build/touch_board_init_test build/touch_coordinates_test build/touch_contact_test build/touch_hid_test build/touch_hid_dump build/touch_pipeline_test build/touch_device_test build/check_chkdsk_test build/eint_test build/gpu_power_test build/gpu_mmu_test build/gpu_fw_objects_test build/gpu_fw_queue_test build/gpu_transfer_stream_test build/gpu_windows_linear_test build/gpu_present_bounds_test build/gpu_dma_pages_test build/gpu_transfer_mapping_test
	./build/gpu_render_pds_data_test
	python3 tests/gpu_render_pds_reconstruct_test.py
	python3 tests/gpu_render_trace_test.py
	python3 tests/gpu_render_fragment_test.py
	python3 tests/gpu_render_usc_test.py
	python3 tests/gpu_render_vdm_pds_test.py
	./build/gpu_windows_render_memory_test
	./build/gpu_render_kick_off
	./build/gpu_render_kick_on
	./build/gpu_windows_render_command_test
	./build/gpu_render_execution_test
	./build/bq24160_handoff_test
	./build/bq24160_charger_test
	./build/bq24160_i2c_test
	./build/gpu_render_context_test
	./build/gpu_render_hwrt_test
	./build/gpu_render_freelist_test
	./build/gpu_render_stream_test
	./build/lc709203f_test
	./build/lc709203f_i2c_test
	./build/msdc_io_gate_test
	./build/toprgu_watchdog_policy_test
	./build/toprgu_reset_command_test
	./build/cdb_test
	./build/pio_test
	./build/scsi_capacity_test
	./build/dma_test
	./build/scsi_test
	python3 tests/gpt_guard_fixtures.py
	./build/button_state_test
	./build/button_pipeline_test
	./build/button_device_test
	./build/button_hardware_test
	./build/pmic_read_test
	python3 tests/update_driver_ntfs_test.py
	./build/bu21025_test
	./build/i2c_pio_test
	./build/bu21025_bus_test
	./build/touch_board_init_test
	./build/touch_coordinates_test
	./build/touch_contact_test
	./build/touch_hid_test
	python3 tests/touch_hid_descriptor_test.py
	./build/touch_pipeline_test
	./build/touch_device_test
	./build/check_chkdsk_test
	./build/eint_test
	./build/gpu_power_test
	./build/gpu_mmu_test
	./build/gpu_fw_objects_test
	./build/gpu_fw_queue_test
	./build/gpu_transfer_stream_test
	./build/gpu_windows_linear_test
	./build/gpu_present_bounds_test
	./build/gpu_dma_pages_test
	./build/gpu_transfer_mapping_test

build/bq24160_handoff_test: drivers/power/bq24160/charger.c drivers/power/bq24160/charger.h tests/bq24160_handoff_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/charger.c tests/bq24160_handoff_test.c -o $@

build/bq24160_charger_test: drivers/power/bq24160/charger.c drivers/power/bq24160/charger.h tests/bq24160_charger_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/charger.c tests/bq24160_charger_test.c -o $@

build/bq24160_i2c_test: drivers/power/bq24160/mt8167_i2c.c drivers/power/bq24160/mt8167_i2c.h drivers/power/lc709203f/mt8167_i2c.h tests/bq24160_i2c_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/mt8167_i2c.c tests/bq24160_i2c_test.c -o $@

build/gpu_render_context_test: drivers/gpu/mt8167/render_context.c drivers/gpu/mt8167/render_context.h drivers/gpu/mt8167/render_pair.c drivers/gpu/mt8167/render_pair.h drivers/gpu/mt8167/render_stream.c drivers/gpu/mt8167/firmware_objects.c tests/gpu_render_context_test.c tests/gpu_render_mesa_fixture.h
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/render_context.c drivers/gpu/mt8167/render_pair.c drivers/gpu/mt8167/render_stream.c drivers/gpu/mt8167/firmware_objects.c tests/gpu_render_context_test.c -o $@

build/gpu_windows_render_memory_test: tests/gpu_windows_render_memory_test.c tests/gpu_windows_stubs/ntddk.h drivers/gpu/mt8167/windows_render_memory.c drivers/gpu/mt8167/windows_render_memory.h drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/render_pds_data.c drivers/gpu/mt8167/render_execution.c
	mkdir -p build
	$(CC) $(CFLAGS) -DA04_GE8300_RENDER_EXPERIMENT=1 -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_windows_render_memory_test.c drivers/gpu/mt8167/windows_render_memory.c drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/render_pds_data.c drivers/gpu/mt8167/render_execution.c -o $@

build/gpu_render_kick_off: tests/gpu_render_kick_policy_test.c drivers/gpu/mt8167/render_kick_policy.h
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_render_kick_policy_test.c -o $@

build/gpu_render_kick_on: tests/gpu_render_kick_policy_test.c drivers/gpu/mt8167/render_kick_policy.h
	mkdir -p build
	$(CC) $(CFLAGS) -DA04_GE8300_RENDER_EXPERIMENT=1 -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_render_kick_policy_test.c -o $@

build/gpu_windows_render_command_test: tests/gpu_windows_render_command_test.c tests/gpu_windows_stubs/ntddk.h drivers/gpu/mt8167/windows_firmware.c drivers/gpu/mt8167/windows_firmware.h drivers/gpu/mt8167/render_kick_policy.h
	mkdir -p build
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -DA04_GE8300_RENDER_EXPERIMENT=1 -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_windows_render_command_test.c drivers/gpu/mt8167/windows_firmware.c -Wl,--gc-sections -o $@

build/gpu_render_pds_data_test: drivers/gpu/mt8167/render_pds_data.c drivers/gpu/mt8167/render_pds_data.h drivers/gpu/mt8167/render_execution.c drivers/gpu/mt8167/render_execution.h tests/gpu_render_pds_data_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/render_pds_data.c drivers/gpu/mt8167/render_execution.c tests/gpu_render_pds_data_test.c -o $@

build/gpu_render_execution_test: drivers/gpu/mt8167/render_execution.c drivers/gpu/mt8167/render_execution.h tests/gpu_render_execution_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/render_execution.c tests/gpu_render_execution_test.c -o $@

build/gpu_render_hwrt_test: drivers/gpu/mt8167/render_hwrt.c drivers/gpu/mt8167/render_hwrt.h drivers/gpu/mt8167/render_target.c drivers/gpu/mt8167/render_target.h drivers/gpu/mt8167/render_freelist.c drivers/gpu/mt8167/firmware_objects.c tests/gpu_render_hwrt_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/render_hwrt.c drivers/gpu/mt8167/render_target.c drivers/gpu/mt8167/render_freelist.c drivers/gpu/mt8167/firmware_objects.c tests/gpu_render_hwrt_test.c -o $@

build/gpu_render_freelist_test: drivers/gpu/mt8167/render_freelist.c drivers/gpu/mt8167/render_freelist.h tests/gpu_render_freelist_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/render_freelist.c tests/gpu_render_freelist_test.c -o $@

build/gpu_render_stream_test: drivers/gpu/mt8167/render_stream.c drivers/gpu/mt8167/render_stream.h tests/gpu_render_stream_test.c tests/gpu_render_mesa_fixture.h
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/render_stream.c tests/gpu_render_stream_test.c -o $@

build/lc709203f_test: drivers/power/lc709203f/gauge.c drivers/power/lc709203f/gauge.h tests/lc709203f_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/lc709203f drivers/power/lc709203f/gauge.c tests/lc709203f_test.c -o $@

build/lc709203f_i2c_test: drivers/power/lc709203f/mt8167_i2c.c drivers/power/lc709203f/mt8167_i2c.h drivers/bus/mt8167-i2c/pio.h tests/lc709203f_i2c_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/lc709203f drivers/power/lc709203f/mt8167_i2c.c tests/lc709203f_i2c_test.c -o $@

build/msdc_io_gate_test: $(SRC)/io_gate.h tests/msdc_io_gate_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -I$(SRC) tests/msdc_io_gate_test.c -o $@

build/toprgu_watchdog_policy_test: drivers/system/mt8167-toprgu/watchdog_policy.h tests/toprgu_watchdog_policy_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/system/mt8167-toprgu tests/toprgu_watchdog_policy_test.c -o $@

build/toprgu_reset_command_test: drivers/system/mt8167-toprgu/reset_command.c drivers/system/mt8167-toprgu/reset_command.h tests/toprgu_reset_command_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/system/mt8167-toprgu drivers/system/mt8167-toprgu/reset_command.c tests/toprgu_reset_command_test.c -o $@

build/gpu_power_test: drivers/gpu/mt8167/rogue_power.h tests/gpu_power_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 tests/gpu_power_test.c -o $@

build/gpu_mmu_test: drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/gpu_mmu.h tests/gpu_mmu_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/gpu_mmu.c tests/gpu_mmu_test.c -o $@

build/gpu_fw_objects_test: drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/firmware_objects.h tests/gpu_fw_objects_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/firmware_objects.c tests/gpu_fw_objects_test.c -o $@

build/gpu_fw_queue_test: drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/firmware_queue.c drivers/gpu/mt8167/firmware_queue.h tests/gpu_fw_queue_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/firmware_queue.c tests/gpu_fw_queue_test.c -o $@

build/gpu_transfer_stream_test: drivers/gpu/mt8167/transfer_stream.c drivers/gpu/mt8167/transfer_stream.h tests/gpu_transfer_stream_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/transfer_stream.c tests/gpu_transfer_stream_test.c -o $@

build/gpu_windows_linear_test: tests/gpu_windows_linear_test.c tests/gpu_windows_stubs/ntddk.h drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/firmware_queue.c drivers/gpu/mt8167/transfer_stream.c drivers/gpu/mt8167/copy_fixture.c drivers/gpu/mt8167/linear_copy_fixture.c drivers/gpu/mt8167/windows_queue_test.c drivers/gpu/mt8167/transfer_mapping.c drivers/gpu/mt8167/windows_transfer_mapping.c drivers/gpu/mt8167/windows_linear_transfer.c drivers/gpu/mt8167/windows_linear_context.c
	mkdir -p build
	$(CC) $(CFLAGS) -DA04_GE8300_LINEAR_TEST=1 -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_windows_linear_test.c drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/firmware_queue.c drivers/gpu/mt8167/transfer_stream.c drivers/gpu/mt8167/copy_fixture.c drivers/gpu/mt8167/linear_copy_fixture.c drivers/gpu/mt8167/windows_queue_test.c drivers/gpu/mt8167/transfer_mapping.c drivers/gpu/mt8167/windows_transfer_mapping.c drivers/gpu/mt8167/windows_linear_transfer.c drivers/gpu/mt8167/windows_linear_context.c -o $@

build/gpu_present_bounds_test: tests/gpu_present_bounds_test.c drivers/gpu/mt8167/wddm-display/present_bounds.h
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/wddm-display tests/gpu_present_bounds_test.c -o $@

build/gpu_dma_pages_test: drivers/gpu/mt8167/dma_pages.c drivers/gpu/mt8167/dma_pages.h tests/gpu_dma_pages_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/dma_pages.c tests/gpu_dma_pages_test.c -o $@

build/gpu_transfer_mapping_test: drivers/gpu/mt8167/transfer_mapping.c drivers/gpu/mt8167/transfer_mapping.h drivers/gpu/mt8167/gpu_mmu.c tests/gpu_transfer_mapping_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/transfer_mapping.c drivers/gpu/mt8167/gpu_mmu.c tests/gpu_transfer_mapping_test.c -o $@

build/eint_test: drivers/bus/mt8167-eint/eint.c drivers/bus/mt8167-eint/eint.h tests/eint_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/bus/mt8167-eint drivers/bus/mt8167-eint/eint.c tests/eint_test.c -o $@

build/check_chkdsk_test: tools/winpe/check-chkdsk.c tools/winpe/check-chkdsk.h tests/check_chkdsk_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Itools/winpe tools/winpe/check-chkdsk.c tests/check_chkdsk_test.c -o $@

build/touch_device_test: drivers/input/bu21025/device.c drivers/input/bu21025/bu21025.c drivers/input/bu21025/pipeline.c drivers/input/bu21025/contact.c drivers/input/bu21025/coordinates.c drivers/input/bu21025/touch_hid.c tests/touch_device_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/device.c drivers/input/bu21025/bu21025.c drivers/input/bu21025/pipeline.c drivers/input/bu21025/contact.c drivers/input/bu21025/coordinates.c drivers/input/bu21025/touch_hid.c tests/touch_device_test.c -o $@

build/touch_pipeline_test: drivers/input/bu21025/pipeline.c drivers/input/bu21025/contact.c drivers/input/bu21025/coordinates.c drivers/input/bu21025/touch_hid.c tests/touch_pipeline_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/pipeline.c drivers/input/bu21025/contact.c drivers/input/bu21025/coordinates.c drivers/input/bu21025/touch_hid.c tests/touch_pipeline_test.c -o $@

build/touch_contact_test: drivers/input/bu21025/contact.c drivers/input/bu21025/contact.h tests/touch_contact_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/contact.c tests/touch_contact_test.c -o $@

build/touch_hid_dump: drivers/input/bu21025/touch_hid.c drivers/input/bu21025/touch_hid.h tests/touch_hid_dump.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/touch_hid.c tests/touch_hid_dump.c -o $@

build/touch_hid_test: drivers/input/bu21025/touch_hid.c drivers/input/bu21025/touch_hid.h tests/touch_hid_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/touch_hid.c tests/touch_hid_test.c -o $@

build/touch_coordinates_test: drivers/input/bu21025/coordinates.c drivers/input/bu21025/coordinates.h tests/touch_coordinates_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/coordinates.c tests/touch_coordinates_test.c -o $@

build/touch_board_init_test: drivers/input/bu21025/board_init.c drivers/input/bu21025/board_init.h tests/touch_board_init_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/board_init.c tests/touch_board_init_test.c -o $@

build/bu21025_bus_test: drivers/input/bu21025/bu21025.c drivers/input/bu21025/mt8167_adapter.c drivers/bus/mt8167-i2c/pio.c tests/bu21025_bus_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/bu21025.c drivers/input/bu21025/mt8167_adapter.c drivers/bus/mt8167-i2c/pio.c tests/bu21025_bus_test.c -o $@

build/i2c_pio_test: drivers/bus/mt8167-i2c/pio.c drivers/bus/mt8167-i2c/pio.h tests/i2c_pio_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/bus/mt8167-i2c drivers/bus/mt8167-i2c/pio.c tests/i2c_pio_test.c -o $@

build/bu21025_test: drivers/input/bu21025/bu21025.c drivers/input/bu21025/bu21025.h tests/bu21025_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/input/bu21025/bu21025.c tests/bu21025_test.c -o $@

test-ntfs-root:
	python3 tests/ntfs_root_test.py
	python3 tests/ntfs_index_bitmap_test.py

build/button_state_test: drivers/input/a04br3-buttons/button_state.c drivers/input/a04br3-buttons/button_state.h tests/button_state_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/a04br3-buttons drivers/input/a04br3-buttons/button_state.c tests/button_state_test.c -o $@

build/button_hardware_test: drivers/input/a04br3-buttons/button_hardware.c drivers/input/a04br3-buttons/button_hardware.h drivers/input/a04br3-buttons/pmic_read.c drivers/input/a04br3-buttons/button_device.c drivers/input/a04br3-buttons/button_pipeline.c drivers/input/a04br3-buttons/button_state.c tests/button_hardware_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/a04br3-buttons drivers/input/a04br3-buttons/button_hardware.c drivers/input/a04br3-buttons/pmic_read.c drivers/input/a04br3-buttons/button_device.c drivers/input/a04br3-buttons/button_pipeline.c drivers/input/a04br3-buttons/button_state.c tests/button_hardware_test.c -o $@

build/button_device_test: drivers/input/a04br3-buttons/button_device.c drivers/input/a04br3-buttons/button_device.h drivers/input/a04br3-buttons/button_pipeline.c drivers/input/a04br3-buttons/button_state.c tests/button_device_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/a04br3-buttons drivers/input/a04br3-buttons/button_device.c drivers/input/a04br3-buttons/button_pipeline.c drivers/input/a04br3-buttons/button_state.c tests/button_device_test.c -o $@

build/pmic_read_test: drivers/input/a04br3-buttons/pmic_read.c drivers/input/a04br3-buttons/pmic_read.h tests/pmic_read_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/a04br3-buttons drivers/input/a04br3-buttons/pmic_read.c tests/pmic_read_test.c -o $@

build/button_pipeline_test: drivers/input/a04br3-buttons/button_pipeline.c drivers/input/a04br3-buttons/button_pipeline.h drivers/input/a04br3-buttons/button_state.c drivers/input/a04br3-buttons/button_state.h tests/button_pipeline_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/a04br3-buttons drivers/input/a04br3-buttons/button_pipeline.c drivers/input/a04br3-buttons/button_state.c tests/button_pipeline_test.c -o $@

build/gpt_guard_test: $(SRC)/gpt_guard.c $(SRC)/gpt_guard.h tests/gpt_guard_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/gpt_guard.c tests/gpt_guard_test.c -o $@

build/cdb_test: $(SRC)/cdb.c $(SRC)/cdb.h tests/cdb_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/cdb.c tests/cdb_test.c -o $@

build/scsi_capacity_test: $(SRC)/scsi_capacity.c $(SRC)/scsi_capacity.h tests/scsi_capacity_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/scsi_capacity.c tests/scsi_capacity_test.c -o $@

build/scsi_test: $(SRC)/scsi.c $(SRC)/scsi.h $(SRC)/cdb.c $(SRC)/scsi_capacity.c tests/scsi_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/scsi.c $(SRC)/cdb.c $(SRC)/scsi_capacity.c tests/scsi_test.c -o $@

build/dma_test: $(SRC)/dma.c $(SRC)/dma.h $(SRC)/pio.h tests/dma_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/dma.c tests/dma_test.c -o $@

build/pio_test: $(SRC)/pio.c $(SRC)/pio.h tests/pio_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/pio.c tests/pio_test.c -o $@

arm-coff: $(SRC)/cdb.c $(SRC)/cdb.h
	mkdir -p build
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/gpu/mt8167/firmware.c -o build/ge8300-firmware-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif -c drivers/gpu/mt8167/firmware_runtime.c -o build/ge8300-runtime-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif -c drivers/gpu/mt8167/firmware_start.c -o build/ge8300-start-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c $(SRC)/cdb.c -o build/cdb-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c $(SRC)/pio.c -o build/pio-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/bu21025.c -o build/bu21025-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/board_init.c -o build/touch-board-init-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/bus/mt8167-i2c/pio.c -o build/i2c-pio-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/mt8167_adapter.c -o build/bu21025-mt8167-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/coordinates.c -o build/touch-coordinates-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/touch_hid.c -o build/touch-hid-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/contact.c -o build/touch-contact-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/pipeline.c -o build/touch-pipeline-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/device.c -o build/touch-device-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/bus/mt8167-eint/eint.c -o build/eint-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/a04br3-buttons/button_pipeline.c -o build/button-pipeline-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/a04br3-buttons/pmic_read.c -o build/pmic-read-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/a04br3-buttons/button_device.c -o build/button-device-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/a04br3-buttons/button_hardware.c -o build/button-hardware-arm.obj

clean:
	rm -rf build

# Linux-derived MT8167 thermal components are separately GPL-2.0-only.
test-thermal: build/thermal_test build/thermal_devinfo_test build/thermal_hardware_test
	./build/thermal_test
	./build/thermal_devinfo_test
	./build/thermal_hardware_test

build/thermal_test: drivers/power/mt8167-thermal/thermal.c drivers/power/mt8167-thermal/thermal.h drivers/power/mt8167-thermal/manager.c drivers/power/mt8167-thermal/manager.h tests/thermal_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/mt8167-thermal drivers/power/mt8167-thermal/thermal.c drivers/power/mt8167-thermal/manager.c tests/thermal_test.c -o $@

build/thermal_devinfo_test: drivers/power/mt8167-thermal/devinfo.c drivers/power/mt8167-thermal/devinfo.h tests/thermal_devinfo_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/mt8167-thermal drivers/power/mt8167-thermal/devinfo.c tests/thermal_devinfo_test.c -o $@

build/thermal_hardware_test: drivers/power/mt8167-thermal/hardware.c drivers/power/mt8167-thermal/hardware.h drivers/power/mt8167-thermal/thermal.c tests/thermal_hardware_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/mt8167-thermal drivers/power/mt8167-thermal/hardware.c drivers/power/mt8167-thermal/thermal.c tests/thermal_hardware_test.c -o $@
