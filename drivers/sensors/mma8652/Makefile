CC ?= cc
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
ROOT := ../../..
OUT := $(ROOT)/build/mma8652
.PHONY: test arm-coff
test:
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -I. mma8652.c $(ROOT)/tests/mma8652_test.c -o $(OUT)/sample-test
	$(CC) $(CFLAGS) -I. mma8652.c $(ROOT)/tests/mma8652_init_test.c -o $(OUT)/init-test
	$(CC) $(CFLAGS) -I. mt8167_i2c.c $(ROOT)/drivers/bus/mt8167-i2c/pio.c $(ROOT)/tests/mma8652_i2c_test.c -o $(OUT)/i2c-test
	$(OUT)/sample-test
	$(OUT)/init-test
	$(OUT)/i2c-test
arm-coff:
	mkdir -p $(OUT)
	$(CLANG) --target=armv7-pc-windows-msvc -ffreestanding -Wall -Wextra -Werror -c mma8652.c -o $(OUT)/mma8652.obj
	$(CLANG) --target=armv7-pc-windows-msvc -ffreestanding -Wall -Wextra -Werror -c sensor_hid.c -o $(OUT)/sensor_hid.obj
	$(CLANG) --target=armv7-pc-windows-msvc -ffreestanding -Wall -Wextra -Werror -c mt8167_i2c.c -o $(OUT)/mt8167_i2c.obj

.PHONY: test-hid
test-hid:
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -I. sensor_hid.c $(ROOT)/tests/mma8652_hid_test.c -o $(OUT)/hid-test
	$(CC) $(CFLAGS) -I. mma8652.c sensor_hid.c $(ROOT)/tests/mma8652_orientation_test.c -o $(OUT)/orientation-test
	$(OUT)/hid-test
	$(OUT)/orientation-test
