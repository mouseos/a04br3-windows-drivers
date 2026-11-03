CC ?= cc
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
SRC := drivers/storage/mt8167-msdc

.PHONY: test test-ntfs-root arm-coff clean

test: build/cdb_test build/pio_test build/scsi_capacity_test build/dma_test build/scsi_test build/gpt_guard_test build/button_state_test build/button_pipeline_test build/pmic_read_test build/bu21025_test build/i2c_pio_test build/bu21025_bus_test build/touch_coordinates_test build/touch_contact_test build/touch_hid_test build/touch_hid_dump build/touch_pipeline_test build/touch_device_test build/check_chkdsk_test build/eint_test
	./build/cdb_test
	./build/pio_test
	./build/scsi_capacity_test
	./build/dma_test
	./build/scsi_test
	python3 tests/gpt_guard_fixtures.py
	./build/button_state_test
	./build/button_pipeline_test
	./build/pmic_read_test
	python3 tests/update_driver_ntfs_test.py
	./build/bu21025_test
	./build/i2c_pio_test
	./build/bu21025_bus_test
	./build/touch_coordinates_test
	./build/touch_contact_test
	./build/touch_hid_test
	python3 tests/touch_hid_descriptor_test.py
	./build/touch_pipeline_test
	./build/touch_device_test
	./build/check_chkdsk_test
	./build/eint_test

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
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c $(SRC)/cdb.c -o build/cdb-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c $(SRC)/pio.c -o build/pio-arm.obj
	$(CLANG) --target=thumbv7-pc-windows-msvc -ffreestanding -fno-stack-protector -Wall -Wextra -Werror -std=c11 -c drivers/input/bu21025/bu21025.c -o build/bu21025-arm.obj
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

clean:
	rm -rf build
