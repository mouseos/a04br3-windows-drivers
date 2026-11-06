CC ?= cc
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
SRC := drivers/storage/mt8167-msdc

.PHONY: test test-thermal test-ntfs-root arm-coff clean

test: build/gpu_mmu_multileaf_test test-mt8167-pwrap-owner test-mt8167-irq-windows test-mt8167-wavert-registration test-mt8167-wavert-miniport test-mt8167-audio-resource
test: test-mma8652 build/msdc_resume_check_test build/msdc_resume_capture_test
test: build/bq24160_owner_broker_test
test: test-mt8167-pnp-backend test-mt8167-audio-adapter-pnp test-mt8167-common-dma test-mt8167-dma-adapter test-mt8167-clock-owner test-mt8167-stream-transport
test: build/gpu_render_shared_size_test test-toprgu-handoff build/gpu_render_context_pds_test
test: test-mt8167-shared-wdf test-mt8167-shared-audio test-mt8167-shared-transport

.PHONY: test-toprgu-handoff
test-toprgu-handoff: build/toprgu_handoff_test
	./build/toprgu_handoff_test

.PHONY: test-mma8652
test-mma8652:
	$(MAKE) -C drivers/sensors/mma8652 test test-hid

test: build/msdc_shared_drain_test build/msdc_touch_shared_test build/msdc_pin_uhs_test build/msdc_parent_resources_test build/msdc_pinmux_test build/msdc_cold_init_test build/msdc_board_owner_test build/msdc_rails_owner_test test-mt8167-pwrap-owner build/msdc_sync_pio_test build/gpu_render_pds_data_test test-mt8167-dl1-engine test-mt8167-wavert-dma build/gpu_windows_render_memory_test build/msdc_lifetime_test build/bqhr_diagnostic_test test-mt8167-audio-broker test-mt8167-audio-ring test-mt8167-audio-irq build/gpu_render_kick_off build/gpu_render_kick_on build/gpu_windows_render_command_test test-mt8167-dl1 build/gpu_render_execution_test test-thermal build/bq24160_handoff_test build/bq24160_charger_test build/bq24160_i2c_test build/gpu_render_context_test build/gpu_render_hwrt_test build/gpu_render_freelist_test build/gpu_render_stream_test build/lc709203f_test build/lc709203f_i2c_test build/msdc_io_gate_test build/toprgu_watchdog_policy_test build/toprgu_reset_command_test build/cdb_test build/pio_test build/scsi_capacity_test build/dma_test build/scsi_test build/gpt_guard_test build/button_state_test build/button_pipeline_test build/button_device_test build/button_hardware_test build/pmic_read_test build/bu21025_test build/i2c_pio_test build/bu21025_bus_test build/touch_board_init_test build/touch_coordinates_test build/touch_contact_test build/touch_hid_test build/touch_hid_dump build/touch_pipeline_test build/touch_device_test build/check_chkdsk_test build/eint_test build/gpu_power_test build/gpu_mmu_test build/gpu_fw_objects_test build/gpu_fw_queue_test build/gpu_transfer_stream_test build/gpu_windows_linear_test build/gpu_present_bounds_test build/gpu_dma_pages_test build/gpu_transfer_mapping_test
	python3 tests/gpu_render_submit_dependencies_test.py
	python3 tests/gpu_render_vertex_pds_code_test.py
	python3 tests/gpu_triangle_nir_audit_test.py
	python3 tests/gpu_render_triangle_fixture_test.py
	python3 tests/gpu_render_fixed_vdm_test.py
	python3 tests/gpu_render_fixed_ppp_test.py
	python3 tests/gpu_render_background_test.py
	python3 tests/gpu_render_spm_load_test.py
	python3 tests/gpu_render_context_static_trace_test.py
	python3 tests/gpu_render_context_bounded_ir_test.py
	python3 tests/gpu_render_shared_log_test.py
	./build/gpu_render_shared_size_test
	./build/gpu_render_context_pds_test
	./build/gpu_render_pds_data_test
	python3 tests/gpu_render_pds_reconstruct_test.py
	python3 tests/gpu_render_trace_test.py
	python3 tests/gpu_render_fragment_test.py
	python3 tests/gpu_render_usc_test.py
	python3 tests/gpu_render_vdm_pds_test.py
	python3 tests/gpu_render_context_trace_test.py
	python3 tests/gpu_render_context_pt_test.py
	python3 tests/gpu_render_context_usc_test.py
	python3 tests/gpu_render_context_bounds_test.py
	python3 tests/gpu_render_context_usc_ir_test.py
	./build/gpu_windows_render_memory_test
	./build/gpu_render_kick_off
	./build/gpu_render_kick_on
	./build/gpu_windows_render_command_test
	./build/gpu_render_execution_test
	./build/bqhr_diagnostic_test
	python3 tests/test_bqhr_acpi_host.py
	./build/bq24160_handoff_test
	./build/bq24160_owner_broker_test
	./build/bq24160_charger_test
	./build/bq24160_i2c_test
	./build/gpu_render_context_test
	./build/gpu_render_hwrt_test
	./build/gpu_render_freelist_test
	./build/gpu_render_stream_test
	python3 tests/gpu_render_capture_audit_test.py
	python3 tests/gpu_render_fixture_plan_test.py
	python3 tests/gpu_render_trace_test.py
	python3 tests/gpu_render_upload_trace_test.py
	python3 tests/gpu_render_upload_candidates_test.py
	python3 tests/gpu_render_csb_summary_test.py
	./build/lc709203f_test
	./build/lc709203f_i2c_test
	./build/msdc_io_gate_test
	./build/msdc_lifetime_test
	./build/msdc_shared_drain_test
	./build/msdc_touch_shared_test
	./build/msdc_pin_uhs_test
	./build/msdc_parent_resources_test
	./build/msdc_pinmux_test
	./build/msdc_cold_init_test
	./build/msdc_board_owner_test
	./build/msdc_rails_owner_test
	./build/msdc_sync_pio_test
	./build/msdc_resume_check_test
	./build/msdc_resume_capture_test
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
	./build/gpu_mmu_multileaf_test
	./build/gpu_fw_objects_test
	./build/gpu_fw_queue_test
	./build/gpu_transfer_stream_test
	./build/gpu_windows_linear_test
	./build/gpu_present_bounds_test
	python3 tests/gpu_copy_d0_lifetime_test.py
	./build/gpu_dma_pages_test
	./build/gpu_transfer_mapping_test

build/bqhr_diagnostic_test: drivers/power/bq24160/bqhr_diagnostic.c drivers/power/bq24160/bqhr_diagnostic.h tests/bqhr_diagnostic_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/bqhr_diagnostic.c tests/bqhr_diagnostic_test.c -o $@

build/bq24160_handoff_test: drivers/power/bq24160/charger.c drivers/power/bq24160/charger.h tests/bq24160_handoff_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/charger.c tests/bq24160_handoff_test.c -o $@

build/bq24160_owner_broker_test: drivers/power/bq24160/owner_broker.c drivers/power/bq24160/owner_broker.h drivers/power/bq24160/charger.c drivers/power/lc709203f/gauge.c tests/bq24160_owner_broker_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/owner_broker.c drivers/power/bq24160/charger.c drivers/power/lc709203f/gauge.c tests/bq24160_owner_broker_test.c -o $@

build/bq24160_charger_test: drivers/power/bq24160/charger.c drivers/power/bq24160/charger.h tests/bq24160_charger_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/charger.c tests/bq24160_charger_test.c -o $@

build/bq24160_i2c_test: drivers/power/bq24160/mt8167_i2c.c drivers/power/bq24160/mt8167_i2c.h drivers/power/lc709203f/mt8167_i2c.h tests/bq24160_i2c_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/bq24160 drivers/power/bq24160/mt8167_i2c.c tests/bq24160_i2c_test.c -o $@

build/gpu_render_context_test: drivers/gpu/mt8167/render_context.c drivers/gpu/mt8167/render_context.h drivers/gpu/mt8167/render_pair.c drivers/gpu/mt8167/render_pair.h drivers/gpu/mt8167/render_stream.c drivers/gpu/mt8167/firmware_objects.c tests/gpu_render_context_test.c tests/gpu_render_mesa_fixture.h
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif drivers/gpu/mt8167/render_context.c drivers/gpu/mt8167/render_pair.c drivers/gpu/mt8167/render_stream.c drivers/gpu/mt8167/firmware_objects.c tests/gpu_render_context_test.c -o $@

build/gpu_windows_render_memory_test: tests/gpu_windows_render_memory_test.c tests/gpu_windows_stubs/ntddk.h drivers/gpu/mt8167/render_fixed_ppp.c drivers/gpu/mt8167/render_fixed_vdm.c drivers/gpu/mt8167/render_pair.c drivers/gpu/mt8167/windows_render_memory.c drivers/gpu/mt8167/windows_render_diagnostic.c drivers/gpu/mt8167/render_fixed_command.c drivers/gpu/mt8167/windows_render_memory.h drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/render_pds_data.c drivers/gpu/mt8167/render_context_pds.c drivers/gpu/mt8167/render_context_static.c drivers/gpu/mt8167/render_context_bounded_usc.h drivers/gpu/mt8167/render_triangle_usc.h drivers/gpu/mt8167/render_shared_size.c drivers/gpu/mt8167/render_shared_size.h drivers/gpu/mt8167/render_execution.c drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/render_context.c drivers/gpu/mt8167/render_freelist.c drivers/gpu/mt8167/render_target.c drivers/gpu/mt8167/render_hwrt.c
	mkdir -p build
	$(CC) $(CFLAGS) -DA04_GE8300_RENDER_EXPERIMENT=1 -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_windows_render_memory_test.c drivers/gpu/mt8167/render_fixed_ppp.c drivers/gpu/mt8167/render_fixed_vdm.c drivers/gpu/mt8167/render_pair.c drivers/gpu/mt8167/windows_render_memory.c drivers/gpu/mt8167/windows_render_diagnostic.c drivers/gpu/mt8167/render_fixed_command.c drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/render_pds_data.c drivers/gpu/mt8167/render_context_pds.c drivers/gpu/mt8167/render_context_static.c drivers/gpu/mt8167/render_shared_size.c drivers/gpu/mt8167/render_execution.c drivers/gpu/mt8167/firmware_objects.c drivers/gpu/mt8167/render_context.c drivers/gpu/mt8167/render_freelist.c drivers/gpu/mt8167/render_target.c drivers/gpu/mt8167/render_hwrt.c -o $@

build/gpu_render_kick_off: tests/gpu_render_kick_policy_test.c drivers/gpu/mt8167/render_kick_policy.h
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_render_kick_policy_test.c -o $@

build/gpu_render_kick_on: tests/gpu_render_kick_policy_test.c drivers/gpu/mt8167/render_kick_policy.h
	mkdir -p build
	$(CC) $(CFLAGS) -DA04_GE8300_RENDER_EXPERIMENT=1 -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_render_kick_policy_test.c -o $@

build/gpu_windows_render_command_test: tests/gpu_windows_render_command_test.c tests/gpu_windows_stubs/ntddk.h drivers/gpu/mt8167/windows_firmware.c drivers/gpu/mt8167/windows_firmware.h drivers/gpu/mt8167/render_kick_policy.h
	mkdir -p build
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -DA04_GE8300_RENDER_EXPERIMENT=1 -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/compat -Idrivers/gpu/mt8167/fwif tests/gpu_windows_render_command_test.c drivers/gpu/mt8167/windows_firmware.c -Wl,--gc-sections -o $@

build/gpu_render_shared_size_test: drivers/gpu/mt8167/render_shared_size.c drivers/gpu/mt8167/render_shared_size.h tests/gpu_render_shared_size_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/render_shared_size.c tests/gpu_render_shared_size_test.c -o $@

build/gpu_render_context_pds_test: drivers/gpu/mt8167/render_context_pds.c drivers/gpu/mt8167/render_context_pds.h drivers/gpu/mt8167/render_execution.c tests/gpu_render_context_pds_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/render_context_pds.c drivers/gpu/mt8167/render_execution.c tests/gpu_render_context_pds_test.c -o $@

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

build/msdc_resume_capture_test: $(SRC)/resume_capture.c $(SRC)/resume_capture.h $(SRC)/resume_check.h $(SRC)/pio.h tests/msdc_resume_capture_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/resume_capture.c tests/msdc_resume_capture_test.c -o $@

build/msdc_resume_check_test: $(SRC)/resume_check.c $(SRC)/resume_check.h tests/msdc_resume_check_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/resume_check.c tests/msdc_resume_check_test.c -o $@

build/msdc_touch_shared_test: drivers/bus/mt8167-shared/register_owner.c drivers/bus/mt8167-shared/register_owner.h drivers/input/bu21025/shared_board.c drivers/input/bu21025/shared_board.h drivers/input/bu21025/board_init.c tests/msdc_touch_shared_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/input/bu21025 drivers/bus/mt8167-shared/register_owner.c drivers/input/bu21025/shared_board.c drivers/input/bu21025/board_init.c tests/msdc_touch_shared_test.c -o $@

build/msdc_pin_uhs_test: $(SRC)/pinmux.c $(SRC)/pinmux.h tests/msdc_pin_uhs_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/pinmux.c tests/msdc_pin_uhs_test.c -o $@

build/msdc_parent_resources_test: drivers/bus/mt8167-shared/parent_resources.c drivers/bus/mt8167-shared/parent_resources.h tests/msdc_parent_resources_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/bus/mt8167-shared drivers/bus/mt8167-shared/parent_resources.c tests/msdc_parent_resources_test.c -o $@

build/msdc_pinmux_test: $(SRC)/pinmux.c $(SRC)/pinmux.h tests/msdc_pinmux_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/pinmux.c tests/msdc_pinmux_test.c -o $@

build/msdc_cold_init_test: $(SRC)/cold_init.c $(SRC)/cold_init.h $(SRC)/resume_capture.c $(SRC)/resume_capture.h tests/msdc_cold_init_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/cold_init.c $(SRC)/resume_capture.c tests/msdc_cold_init_test.c -o $@

build/msdc_board_owner_test: $(SRC)/board_owner.c $(SRC)/board_owner.h tests/msdc_board_owner_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/board_owner.c tests/msdc_board_owner_test.c -o $@

build/msdc_rails_owner_test: drivers/bus/mt8167-shared/pwrap_owner.c drivers/bus/mt8167-shared/pwrap_owner.h drivers/input/a04br3-buttons/pmic_read.c drivers/input/a04br3-buttons/pmic_read.h tests/msdc_rails_owner_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/bus/mt8167-shared drivers/bus/mt8167-shared/pwrap_owner.c drivers/input/a04br3-buttons/pmic_read.c tests/msdc_rails_owner_test.c -o $@

build/msdc_sync_pio_test: $(SRC)/sync_pio.c $(SRC)/sync_pio.h $(SRC)/pio.h tests/msdc_sync_pio_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(SRC) $(SRC)/sync_pio.c tests/msdc_sync_pio_test.c -o $@

build/msdc_lifetime_test: $(SRC)/lifetime.h tests/msdc_lifetime_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -pthread -I$(SRC) tests/msdc_lifetime_test.c -o $@

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
test-thermal: build/thermal_test build/thermal_devinfo_test build/thermal_hardware_test build/thermal_freshness_test
	./build/thermal_test
	./build/thermal_devinfo_test
	./build/thermal_hardware_test
	./build/thermal_freshness_test
	python3 tests/thermal_fdt_test.py

build/thermal_test: drivers/power/mt8167-thermal/thermal.c drivers/power/mt8167-thermal/thermal.h drivers/power/mt8167-thermal/manager.c drivers/power/mt8167-thermal/manager.h tests/thermal_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/mt8167-thermal drivers/power/mt8167-thermal/thermal.c drivers/power/mt8167-thermal/manager.c tests/thermal_test.c -o $@

build/thermal_devinfo_test: drivers/power/mt8167-thermal/devinfo.c drivers/power/mt8167-thermal/devinfo.h tests/thermal_devinfo_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/mt8167-thermal drivers/power/mt8167-thermal/devinfo.c tests/thermal_devinfo_test.c -o $@

build/thermal_hardware_test: drivers/power/mt8167-thermal/hardware.c drivers/power/mt8167-thermal/hardware.h drivers/power/mt8167-thermal/thermal.c tests/thermal_hardware_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/mt8167-thermal drivers/power/mt8167-thermal/hardware.c drivers/power/mt8167-thermal/thermal.c tests/thermal_hardware_test.c -o $@

.PHONY: test-mt8167-dl1 arm-mt8167-dl1
test-mt8167-dl1: build/mt8167_dl1_test
	./build/mt8167_dl1_test
build/mt8167_dl1_test: drivers/audio/mt8167-afe/dl1.c drivers/audio/mt8167-afe/dl1.h tests/mt8167_dl1_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/dl1.c tests/mt8167_dl1_test.c -o $@
arm-mt8167-dl1:
	mkdir -p build
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/audio/mt8167-afe/dl1.c -o build/mt8167-dl1-arm.obj

.PHONY: test-mt8167-audio-irq
test-mt8167-audio-irq: build/mt8167_audio_irq_test
	./build/mt8167_audio_irq_test
build/mt8167_audio_irq_test: drivers/audio/mt8167-afe/irq.c drivers/audio/mt8167-afe/irq.h tests/mt8167_audio_irq_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/irq.c tests/mt8167_audio_irq_test.c -o $@

.PHONY: test-mt8167-audio-ring
test-mt8167-audio-ring: build/mt8167_audio_ring_test
	./build/mt8167_audio_ring_test
build/mt8167_audio_ring_test: drivers/audio/mt8167-afe/ring.c drivers/audio/mt8167-afe/ring.h tests/mt8167_audio_ring_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/ring.c tests/mt8167_audio_ring_test.c -o $@

.PHONY: test-mt8167-audio-broker
test-mt8167-audio-broker: build/mt8167_audio_broker_test
	./build/mt8167_audio_broker_test
build/mt8167_audio_broker_test: drivers/audio/mt8167-afe/broker_policy.c drivers/audio/mt8167-afe/broker_policy.h tests/mt8167_audio_broker_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/broker_policy.c tests/mt8167_audio_broker_test.c -o $@

.PHONY: test-mt8167-dl1-engine
test-mt8167-dl1-engine: build/mt8167_dl1_engine_test
	./build/mt8167_dl1_engine_test
build/mt8167_dl1_engine_test: drivers/audio/mt8167-afe/dl1_engine.c drivers/audio/mt8167-afe/dl1.c tests/mt8167_dl1_engine_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/dl1_engine.c drivers/audio/mt8167-afe/dl1.c tests/mt8167_dl1_engine_test.c -o $@

.PHONY: test-mt8167-wavert-dma
test-mt8167-wavert-dma: build/mt8167_wavert_dma_test
	./build/mt8167_wavert_dma_test
build/mt8167_wavert_dma_test: drivers/audio/mt8167-afe/wavert_dma.cpp drivers/audio/mt8167-afe/wavert_dma.h tests/mt8167_wavert_dma_test.cpp tests/audio_windows_stubs/portcls.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/wavert_dma.cpp tests/mt8167_wavert_dma_test.cpp -o $@

build/thermal_freshness_test: drivers/power/mt8167-thermal/freshness.c drivers/power/mt8167-thermal/freshness.h tests/thermal_freshness_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/power/mt8167-thermal drivers/power/mt8167-thermal/freshness.c tests/thermal_freshness_test.c -o $@
.PHONY: test-mt8167-pwrap-owner
test-mt8167-pwrap-owner: build/mt8167_pwrap_owner_test
	./build/mt8167_pwrap_owner_test
build/mt8167_pwrap_owner_test: drivers/bus/mt8167-shared/pwrap_owner.c drivers/bus/mt8167-shared/pwrap_owner.h drivers/input/a04br3-buttons/pmic_read.c tests/mt8167_pwrap_owner_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Idrivers/bus/mt8167-shared drivers/bus/mt8167-shared/pwrap_owner.c drivers/input/a04br3-buttons/pmic_read.c tests/mt8167_pwrap_owner_test.c -o $@

.PHONY: test-mt8167-irq-windows
test-mt8167-irq-windows: build/mt8167_irq_windows_test
	./build/mt8167_irq_windows_test
build/mt8167_irq_windows_test: drivers/audio/mt8167-afe/irq_windows.cpp drivers/audio/mt8167-afe/irq_windows.h tests/mt8167_irq_windows_test.cpp tests/audio_windows_stubs/ntddk.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/irq_windows.cpp tests/mt8167_irq_windows_test.cpp -o $@

.PHONY: test-mt8167-wavert-registration
test-mt8167-wavert-registration: build/mt8167_wavert_registration_test
	./build/mt8167_wavert_registration_test
build/mt8167_wavert_registration_test: drivers/audio/mt8167-afe/wavert_registration.cpp drivers/audio/mt8167-afe/wavert_registration.h tests/mt8167_wavert_registration_test.cpp tests/audio_windows_stubs/portcls.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -DA04_TEST_REGISTRATION=1 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/wavert_registration.cpp tests/mt8167_wavert_registration_test.cpp -o $@

.PHONY: test-mt8167-wavert-miniport
test-mt8167-wavert-miniport: build/mt8167_wavert_miniport_test
	./build/mt8167_wavert_miniport_test
build/mt8167_wavert_miniport_test: drivers/audio/mt8167-afe/wavert_miniport.cpp drivers/audio/mt8167-afe/wavert_dma.cpp tests/mt8167_wavert_miniport_test.cpp tests/audio_windows_stubs/portcls.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -Wno-multichar -Wno-misleading-indentation -std=c++11 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -DA04_TEST_MINIPORT=1 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/wavert_miniport.cpp drivers/audio/mt8167-afe/wavert_dma.cpp tests/mt8167_wavert_miniport_test.cpp -o $@

.PHONY: test-mt8167-audio-resource
test-mt8167-audio-resource: build/mt8167_audio_resource_test
	./build/mt8167_audio_resource_test
build/mt8167_audio_resource_test: drivers/audio/mt8167-afe/resource_check.h tests/mt8167_audio_resource_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe tests/mt8167_audio_resource_test.c -o $@

.PHONY: test-mt8167-common-dma
test-mt8167-common-dma: build/mt8167_common_dma_test
	./build/mt8167_common_dma_test
build/mt8167_common_dma_test: drivers/audio/mt8167-afe/common_dma.cpp drivers/audio/mt8167-afe/common_dma.h tests/mt8167_common_dma_test.cpp tests/audio_windows_stubs/ntddk.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -DA04_TEST_COMMON_DMA=1 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/common_dma.cpp tests/mt8167_common_dma_test.cpp -o $@

.PHONY: test-mt8167-dma-adapter
test-mt8167-dma-adapter: build/mt8167_dma_adapter_test
	./build/mt8167_dma_adapter_test
build/mt8167_dma_adapter_test: drivers/audio/mt8167-afe/dma_adapter.cpp drivers/audio/mt8167-afe/dma_adapter.h tests/mt8167_dma_adapter_test.cpp tests/audio_windows_stubs/ntddk.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -DA04_TEST_COMMON_DMA=1 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/dma_adapter.cpp tests/mt8167_dma_adapter_test.cpp -o $@

.PHONY: test-mt8167-clock-owner
test-mt8167-clock-owner: build/mt8167_clock_owner_test
	./build/mt8167_clock_owner_test
build/mt8167_clock_owner_test: drivers/bus/mt8167-shared/clock_owner.c drivers/bus/mt8167-shared/clock_owner.h tests/mt8167_clock_owner_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/bus/mt8167-shared drivers/bus/mt8167-shared/clock_owner.c tests/mt8167_clock_owner_test.c -o $@

.PHONY: test-mt8167-stream-transport
test-mt8167-stream-transport: build/mt8167_stream_transport_test
	./build/mt8167_stream_transport_test
build/mt8167_stream_transport_test: drivers/audio/mt8167-afe/stream_transport.cpp drivers/audio/mt8167-afe/stream_transport.h drivers/audio/mt8167-afe/dl1.c tests/mt8167_stream_transport_test.cpp
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe -c drivers/audio/mt8167-afe/dl1.c -o build/mt8167_dl1_host.o
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -DA04_TEST_COMMON_DMA=1 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/stream_transport.cpp tests/mt8167_stream_transport_test.cpp build/mt8167_dl1_host.o -o $@

build/toprgu_handoff_test: drivers/system/mt8167-toprgu/handoff_policy.h tests/toprgu_handoff_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/system/mt8167-toprgu tests/toprgu_handoff_test.c -o $@

.PHONY: test-mt8167-pnp-backend
test-mt8167-pnp-backend: build/mt8167_pnp_backend_test
	./build/mt8167_pnp_backend_test
build/mt8167_pnp_backend_test: drivers/audio/mt8167-afe/pnp_backend.cpp drivers/audio/mt8167-afe/pnp_backend.h drivers/audio/mt8167-afe/common_dma.cpp drivers/audio/mt8167-afe/dma_adapter.cpp tests/mt8167_pnp_backend_test.cpp tests/audio_owner_stubs/portcls.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -Wno-multichar -std=c++11 -DA04_TEST_COMMON_DMA=1 -DA04_TEST_PNP_BACKEND=1 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -Itests/audio_owner_stubs -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/pnp_backend.cpp drivers/audio/mt8167-afe/common_dma.cpp drivers/audio/mt8167-afe/dma_adapter.cpp drivers/audio/mt8167-afe/shared_audio.c tests/mt8167_pnp_backend_test.cpp -o $@

.PHONY: test-mt8167-audio-adapter-pnp
test-mt8167-audio-adapter-pnp: build/mt8167_audio_adapter_pnp_test
	./build/mt8167_audio_adapter_pnp_test
build/mt8167_audio_adapter_pnp_test: drivers/audio/mt8167-afe/adapter.cpp tests/mt8167_audio_adapter_pnp_test.cpp tests/audio_adapter_stubs/portcls.h
	mkdir -p build
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -DA04_MT8167_WAVERT_DIAGNOSTIC=1 -DA04_MT8167_OWNER_DIAGNOSTIC=1 -Itests/audio_adapter_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/adapter.cpp tests/mt8167_audio_adapter_pnp_test.cpp -o $@

.PHONY: test-mt8167-shared-wdf
test-mt8167-shared-wdf: build/mt8167_shared_wdf_test
	./build/mt8167_shared_wdf_test
	python3 tests/shared_parent_prototype_test.py
build/mt8167_shared_wdf_test: tests/mt8167_shared_wdf_test.c tests/shared_wdf_stubs/ntddk.h tests/shared_wdf_stubs/wdf.h drivers/bus/mt8167-shared/wdf_parent_probe.c drivers/bus/mt8167-shared/client_interface.h drivers/bus/mt8167-shared/shared_interface.h drivers/bus/mt8167-shared/audio_interface.h drivers/audio/mt8167-afe/audio_parent_client.c drivers/audio/mt8167-afe/shared_audio.c drivers/bus/mt8167-shared/parent_resources.c drivers/bus/mt8167-shared/pwrap_owner.c drivers/bus/mt8167-shared/register_owner.c drivers/bus/mt8167-shared/register_owner.h drivers/input/a04br3-buttons/pmic_read.c
	mkdir -p build
	$(CC) $(CFLAGS) -DA04_SHARED_QUERY_EXPERIMENTAL=1 -DA04_SHARED_CHILD_PROBE=1 -Itests/shared_wdf_stubs drivers/bus/mt8167-shared/parent_resources.c drivers/bus/mt8167-shared/pwrap_owner.c drivers/bus/mt8167-shared/register_owner.c drivers/input/a04br3-buttons/pmic_read.c drivers/audio/mt8167-afe/audio_parent_client.c drivers/audio/mt8167-afe/shared_audio.c tests/mt8167_shared_wdf_test.c -o $@

build/msdc_shared_drain_test: drivers/bus/mt8167-shared/register_owner.c drivers/bus/mt8167-shared/register_owner.h tests/msdc_shared_drain_test.c
	@mkdir -p build
	$(CC) $(CFLAGS) -pthread -Idrivers/bus/mt8167-shared drivers/bus/mt8167-shared/register_owner.c tests/msdc_shared_drain_test.c -o $@

.PHONY: test-mt8167-shared-audio
test-mt8167-shared-audio: build/mt8167_shared_audio_test
	./build/mt8167_shared_audio_test
build/mt8167_shared_audio_test: drivers/audio/mt8167-afe/shared_audio.c drivers/audio/mt8167-afe/shared_audio.h tests/mt8167_shared_audio_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/shared_audio.c tests/mt8167_shared_audio_test.c -o $@

.PHONY: test-mt8167-shared-transport
test-mt8167-shared-transport: build/mt8167_shared_transport_test
	./build/mt8167_shared_transport_test
build/mt8167_shared_transport_test: drivers/audio/mt8167-afe/shared_transport.cpp drivers/audio/mt8167-afe/shared_audio.c tests/mt8167_shared_transport_test.cpp
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/audio/mt8167-afe -c drivers/audio/mt8167-afe/shared_audio.c -o build/mt8167_shared_audio_host.o
	$(CXX) -O2 -Wall -Wextra -Werror -std=c++11 -DA04_TEST_COMMON_DMA=1 -DA04_MT8167_WAVERT_EXPERIMENTAL=1 -Itests/audio_windows_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/shared_transport.cpp tests/mt8167_shared_transport_test.cpp build/mt8167_shared_audio_host.o -o $@

.PHONY: test-mt8167-audio-parent-client
test-mt8167-audio-parent-client: build/mt8167_audio_parent_client_test
	./build/mt8167_audio_parent_client_test
build/mt8167_audio_parent_client_test: drivers/audio/mt8167-afe/audio_parent_client.c drivers/audio/mt8167-afe/shared_audio.c tests/mt8167_audio_parent_client_test.c drivers/bus/mt8167-shared/audio_interface.h
	mkdir -p build
	$(CC) $(CFLAGS) -Itests/shared_wdf_stubs -Idrivers/audio/mt8167-afe drivers/audio/mt8167-afe/audio_parent_client.c drivers/audio/mt8167-afe/shared_audio.c tests/mt8167_audio_parent_client_test.c -o $@

.PHONY: test-gpu-vendor-m1-11
test-gpu-vendor-m1-11: build/ge8300_display_resource_check_test build/ge8300_windows_display_scanout_test build/ge8300_scanout_lifetime_test build/ge8300_scanout_candidate_test build/gpu_vendor_windows_surface_test build/gpu_vendor_m1_11_stage_test build/gpu_vendor_firmware_requests_test build/gpu_vendor_uncached_test build/gpu_vendor_rebase_test build/gpu_vendor_render_resources_test build/gpu_vendor_windows_render_cold_test build/gpu_vendor_windows_render_context_test build/gpu_vendor_clear_image_test build/gpu_vendor_clear_tile_test build/gpu_vendor_clear_validate_test
	./build/ge8300_scanout_candidate_test
	./build/ge8300_scanout_lifetime_test
	./build/ge8300_windows_display_scanout_test
	./build/ge8300_display_resource_check_test
	python3 tests/gpu_vendor_m1_11_fixture.py
	./build/gpu_vendor_windows_surface_test
	./build/gpu_vendor_m1_11_stage_test build/vendor-m1-11-test/firmware.bin build/vendor-m1-11-test/code.bin build/vendor-m1-11-test/data.bin
	./build/gpu_vendor_firmware_requests_test
	./build/gpu_vendor_uncached_test
	./build/gpu_vendor_rebase_test
	./build/gpu_vendor_render_resources_test
	./build/gpu_vendor_windows_render_cold_test
	./build/gpu_vendor_windows_render_context_test
	./build/gpu_vendor_clear_image_test
	./build/gpu_vendor_clear_tile_test
	./build/gpu_vendor_clear_validate_test

build/ge8300_display_resource_check_test: tests/ge8300_display_resource_check_test.c drivers/gpu/mt8167/wddm-display/ge8300_display_resource_check.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/ge8300_display_resource_check_test.c -o $@

build/ge8300_windows_display_scanout_test: tests/ge8300_windows_display_scanout_test.c drivers/gpu/mt8167/vendor_m1_11/windows_display_scanout.c drivers/gpu/mt8167/vendor_m1_11/windows_display_scanout.h tests/gpu_windows_stubs/ntddk.h
	@mkdir -p build
	$(CC) $(CFLAGS) -DA04_TEST_DISPLAY_SCANOUT -DA04_GE8300_DISPLAY_SCANOUT_EXPERIMENT=1 -Itests/gpu_windows_stubs $(filter %.c,$^) -o $@

build/ge8300_scanout_lifetime_test: tests/ge8300_scanout_lifetime_test.c drivers/gpu/mt8167/vendor_m1_11/scanout_lifetime.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/ge8300_scanout_lifetime_test.c -o $@

build/ge8300_scanout_candidate_test: tests/ge8300_scanout_candidate_test.c drivers/gpu/mt8167/vendor_m1_11/scanout_candidate.h
	@mkdir -p build
	$(CC) $(CFLAGS) tests/ge8300_scanout_candidate_test.c -o $@

build/gpu_vendor_clear_image_test: tests/gpu_vendor_clear_image_test.c drivers/gpu/mt8167/vendor_m1_11/clear_image.c drivers/gpu/mt8167/vendor_m1_11/clear_image.h drivers/gpu/mt8167/vendor_m1_11/render_resources.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/vendor_m1_11 $(filter %.c,$^) -o $@

build/gpu_vendor_windows_render_context_test: tests/gpu_vendor_windows_render_context_test.c drivers/gpu/mt8167/vendor_m1_11/windows_render_context.c drivers/gpu/mt8167/vendor_m1_11/windows_render_clear.c drivers/gpu/mt8167/vendor_m1_11/native_copy_state.c drivers/gpu/mt8167/vendor_m1_11/clear_image.c drivers/gpu/mt8167/vendor_m1_11/windows_render_cold.h drivers/gpu/mt8167/vendor_m1_11/render_resources.c drivers/gpu/mt8167/vendor_m1_11/clear_tile.c
	@mkdir -p build
	$(CC) $(CFLAGS) -DA04_TEST_INTERLOCK_HOOK -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/vendor_m1_11 $(filter %.c,$^) -o $@

build/gpu_vendor_windows_render_cold_test: tests/gpu_vendor_windows_render_cold_test.c drivers/gpu/mt8167/vendor_m1_11/windows_copy_scratch.c drivers/gpu/mt8167/vendor_m1_11/native_copy_state.c drivers/gpu/mt8167/vendor_m1_11/native_copy_usc.c drivers/gpu/mt8167/vendor_m1_11/native_copy_usc.h drivers/gpu/mt8167/vendor_m1_11/windows_render_cold.c drivers/gpu/mt8167/vendor_m1_11/windows_render_cold.h drivers/gpu/mt8167/vendor_m1_11/render_resources.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.c drivers/gpu/mt8167/vendor_m1_11/clear_image.c drivers/gpu/mt8167/vendor_m1_11/windows_surface.c drivers/gpu/mt8167/gpu_mmu.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/vendor_m1_11 $(filter %.c,$^) -o $@

build/gpu_vendor_render_resources_test: tests/gpu_vendor_render_resources_test.c drivers/gpu/mt8167/vendor_m1_11/render_resources.c drivers/gpu/mt8167/vendor_m1_11/render_resources.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueRuntimeLayout.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/vendor_m1_11 tests/gpu_vendor_render_resources_test.c drivers/gpu/mt8167/vendor_m1_11/render_resources.c -o $@

build/gpu_vendor_rebase_test: tests/gpu_vendor_rebase_test.c drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueRuntimeRebase.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueRuntimeLayout.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/vendor_m1_11 $< -o $@

build/gpu_vendor_uncached_test: tests/gpu_vendor_uncached_test.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167/vendor_m1_11 tests/gpu_vendor_uncached_test.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.c -o $@

build/gpu_vendor_firmware_requests_test: tests/gpu_vendor_firmware_requests_test.c drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueFirmwareRequests.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueCommand.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueDeviceWords.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/vendor_m1_11 $< -o $@
	python3 tests/test_ge8300_power_acpi.py

build/gpu_vendor_m1_11_stage_test: tests/gpu_vendor_m1_11_stage_test.c drivers/gpu/mt8167/vendor_m1_11/stage.c drivers/gpu/mt8167/vendor_m1_11/windows_stage.c drivers/gpu/mt8167/vendor_m1_11/windows_start.c drivers/gpu/mt8167/vendor_m1_11/windows_command.c drivers/gpu/mt8167/vendor_m1_11/windows_command.h drivers/gpu/mt8167/vendor_m1_11/windows_irq_snapshot.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueFirmwareRequests.h drivers/gpu/mt8167/vendor_m1_11/windows_stop.c drivers/gpu/mt8167/vendor_m1_11/windows_stop.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueStop.h drivers/gpu/mt8167/vendor_m1_11/stage.h drivers/gpu/mt8167/vendor_m1_11/windows_stage.h drivers/gpu/mt8167/vendor_m1_11/windows_start.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueCommand.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueDeviceWords.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueStartupPlan.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueStartupExecutor.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueFirmware.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueMipsMMU.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueRuntimeLayout.h drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueDDKABI.h drivers/gpu/mt8167/vendor_m1_11/windows_stage_uncached.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.c drivers/gpu/mt8167/vendor_m1_11/A04BR3RogueRuntimeRebase.h
	mkdir -p build
	$(CC) $(CFLAGS) -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/vendor_m1_11 tests/gpu_vendor_m1_11_stage_test.c drivers/gpu/mt8167/vendor_m1_11/stage.c drivers/gpu/mt8167/vendor_m1_11/windows_stage.c drivers/gpu/mt8167/vendor_m1_11/windows_start.c drivers/gpu/mt8167/vendor_m1_11/windows_command.c drivers/gpu/mt8167/vendor_m1_11/windows_stop.c drivers/gpu/mt8167/vendor_m1_11/windows_stage_uncached.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.c -o $@

test: test-gpu-vendor-m1-11

build/gpu_vendor_clear_tile_test: tests/gpu_vendor_clear_tile_test.c drivers/gpu/mt8167/vendor_m1_11/clear_tile.c drivers/gpu/mt8167/vendor_m1_11/clear_tile.h drivers/gpu/mt8167/vendor_m1_11/clear_image.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/vendor_m1_11 drivers/gpu/mt8167/vendor_m1_11/clear_image.c drivers/gpu/mt8167/vendor_m1_11/clear_tile.c tests/gpu_vendor_clear_tile_test.c -o $@

build/gpu_vendor_clear_validate_test: tests/gpu_vendor_clear_validate_test.c drivers/gpu/mt8167/vendor_m1_11/clear_validate.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/vendor_m1_11 tests/gpu_vendor_clear_validate_test.c -o $@

build/gpu_mmu_multileaf_test: tests/gpu_mmu_multileaf_test.c drivers/gpu/mt8167/gpu_mmu.c drivers/gpu/mt8167/gpu_mmu.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167 drivers/gpu/mt8167/gpu_mmu.c tests/gpu_mmu_multileaf_test.c -o $@

build/gpu_vendor_windows_surface_test: tests/gpu_vendor_windows_surface_test.c drivers/gpu/mt8167/vendor_m1_11/windows_surface.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.c drivers/gpu/mt8167/gpu_mmu.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests/gpu_windows_stubs -Idrivers/gpu/mt8167 -Idrivers/gpu/mt8167/vendor_m1_11 tests/gpu_vendor_windows_surface_test.c drivers/gpu/mt8167/vendor_m1_11/windows_surface.c drivers/gpu/mt8167/vendor_m1_11/windows_uncached.c drivers/gpu/mt8167/gpu_mmu.c -o $@

.PHONY: test-gpu-wddm-staged-present
test-gpu-wddm-staged-present: build/gpu_wddm_staged_present_test
	./build/gpu_wddm_staged_present_test

build/gpu_wddm_staged_present_test: tests/gpu_wddm_staged_present_test.cpp drivers/gpu/mt8167/wddm-display/ge8300_present.cxx drivers/gpu/mt8167/wddm-display/present_bounds.h
	mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Wno-multichar $< -o $@

.PHONY: test-gpu-vendor-native-copy-state
test-gpu-vendor-native-copy-state: build/gpu_vendor_native_copy_state_test
	./build/gpu_vendor_native_copy_state_test

build/gpu_vendor_native_copy_state_test: tests/gpu_vendor_native_copy_state_test.c drivers/gpu/mt8167/vendor_m1_11/native_copy_state.c drivers/gpu/mt8167/vendor_m1_11/native_copy_state.h
	mkdir -p build
	$(CC) $(CFLAGS) -Idrivers/gpu/mt8167/vendor_m1_11 tests/gpu_vendor_native_copy_state_test.c drivers/gpu/mt8167/vendor_m1_11/native_copy_state.c -o $@
