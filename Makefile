CC ?= cc
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
SRC := drivers/storage/mt8167-msdc

.PHONY: test arm-coff clean

test: build/cdb_test build/pio_test build/scsi_capacity_test build/dma_test build/scsi_test build/gpt_guard_test
	./build/cdb_test
	./build/pio_test
	./build/scsi_capacity_test
	./build/dma_test
	./build/scsi_test
	python3 tests/gpt_guard_fixtures.py

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

clean:
	rm -rf build
