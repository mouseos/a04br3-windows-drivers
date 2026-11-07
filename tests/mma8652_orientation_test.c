/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mma8652.h"
#include "sensor_hid.h"

struct frame { uint8_t data[7]; unsigned reads; };
static int read_frame(void *context, uint8_t address, uint8_t reg,
                      uint8_t *out, unsigned bytes)
{
    struct frame *f = context;
    assert(address == 0x1d && reg == 0 && bytes == sizeof(f->data));
    ++f->reads;
    memcpy(out, f->data, bytes);
    return 0;
}
static void encode(uint8_t *out, int value)
{
    uint16_t raw = (uint16_t)((unsigned)(value & 0xfff) << 4);
    out[0] = (uint8_t)(raw >> 8); out[1] = (uint8_t)raw;
}
static int signed16(const uint8_t *p)
{
    unsigned value = p[0] | ((unsigned)p[1] << 8);
    return value & 0x8000u ? (int)value - 65536 : (int)value;
}
int main(void)
{
    /* Independent raw -> Linux direction-7 board -> Windows panel fixtures.
     * The four in-plane basis vectors catch the 180-degree regression in
     * 0.0.1.7; +/- Z must retain the existing face-up/down convention.
     * These are axis fixtures, not new measurements of physical poses. */
    static const struct {
        int raw[3], board[3], hid[3];
    } cases[] = {
        {{ 1024,0,0}, {0,-1024,0}, {-100,0,0}},
        {{-1024,0,0}, {0, 1024,0}, { 100,0,0}},
        {{0, 1024,0}, {-1024,0,0}, {0, 100,0}},
        {{0,-1024,0}, { 1024,0,0}, {0,-100,0}},
        {{0,0, 1024}, {0,0,-1024}, {0,0,-100}},
        {{0,0,-1024}, {0,0, 1024}, {0,0, 100}},
        {{-2048,2047,-1024}, {-2047,2048,1024}, {200,200,100}},
        {{2047,-2048,0}, {2048,-2047,0}, {-200,-200,0}},
    };
    unsigned i;
    for (i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        struct frame f = {{8,0,0,0,0,0,0},0};
        struct a04_mma8652_bus bus = {&f,read_frame,0};
        struct a04_mma8652_sample sample;
        uint8_t report[A04_MMA_HID_INPUT_SIZE];
        encode(f.data+1,cases[i].raw[0]);
        encode(f.data+3,cases[i].raw[1]);
        encode(f.data+5,cases[i].raw[2]);
        assert(a04_mma8652_read(&bus,&sample) == A04_MMA_OK && f.reads == 1);
        assert(sample.x == cases[i].board[0]);
        assert(sample.y == cases[i].board[1]);
        assert(sample.z == cases[i].board[2]);
        a04_mma_hid_input(&sample,report);
        assert(report[0] == 1 && report[1] == 2 && report[2] == 4);
        assert(signed16(report+3) == cases[i].hid[0]);
        assert(signed16(report+5) == cases[i].hid[1]);
        assert(signed16(report+7) == cases[i].hid[2]);
    }
    puts("MMA8652 raw/board/panel orientation tests passed");
    return 0;
}
