/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Read path adapted from the TAB-A04-BR3 EDK2 SdCardBlockIoDxe driver,
 * https://github.com/mouseos/edk2-mtk, Platform/Benesse/A04BR3Pkg.
 */
#include "pio.h"

#define INT_STATUS 0x00cu
#define FIFO_STATUS 0x014u
#define RX_DATA 0x01cu
#define COMMAND 0x034u
#define ARGUMENT 0x038u
#define STATUS 0x03cu
#define RESPONSE0 0x040u
#define BLOCK_COUNT 0x050u
#define FIFO_CLEAR (1u << 31)
#define COMMAND_READY (1u << 8)
#define COMMAND_TIMEOUT (1u << 9)
#define RESPONSE_CRC (1u << 10)
#define TRANSFER_COMPLETE (1u << 12)
#define DATA_TIMEOUT (1u << 14)
#define DATA_CRC (1u << 15)
#define COMMAND_BITS (COMMAND_READY | COMMAND_TIMEOUT | RESPONSE_CRC)
#define DATA_BITS (TRANSFER_COMPLETE | DATA_TIMEOUT | DATA_CRC)
#define CARD_R1_ERRORS 0xffff0000u
#define CMD17_SINGLE_R1_512 (17u | (1u << 7) | (1u << 11) | (512u << 16))

/* One command at a time; DMA, concurrent requests and ISR ownership are not
 * supported. The wrapper must serialize and quiesce them before calling. */
enum a04_msdc_result a04_msdc_read_sector(const struct a04_msdc_io *io,
                                          uint32_t lba, uint8_t *buffer,
                                          size_t length, uint32_t poll_limit)
{
    uint32_t n, bits, count, val;
    size_t offset = 0;
    int done = 0;
    if (!io || !io->read32 || !io->read8 || !io->write32 || !buffer ||
        length != 512 || !poll_limit)
        return A04_MSDC_INVALID;
    for (n = 0; n < poll_limit; ++n)
        if (!(io->read32(io->context, STATUS) & 1u)) break;
    if (n == poll_limit) return A04_MSDC_BUSY_TIMEOUT;
    io->write32(io->context, BLOCK_COUNT, 1);
    val = io->read32(io->context, FIFO_STATUS);
    io->write32(io->context, FIFO_STATUS, val | FIFO_CLEAR);
    for (n = 0; n < poll_limit; ++n)
        if (!(io->read32(io->context, FIFO_STATUS) & FIFO_CLEAR)) break;
    if (n == poll_limit) return A04_MSDC_FIFO_TIMEOUT;
    bits = io->read32(io->context, INT_STATUS) & (COMMAND_BITS | DATA_BITS);
    if (bits) io->write32(io->context, INT_STATUS, bits);
    io->write32(io->context, ARGUMENT, lba);
    io->write32(io->context, COMMAND, CMD17_SINGLE_R1_512);
    for (n = 0; n < poll_limit; ++n) {
        bits = io->read32(io->context, INT_STATUS) & COMMAND_BITS;
        if (!bits) continue;
        io->write32(io->context, INT_STATUS, bits);
        if (bits & COMMAND_TIMEOUT) return A04_MSDC_COMMAND_TIMEOUT;
        if (bits & RESPONSE_CRC) return A04_MSDC_COMMAND_CRC;
        if (bits & COMMAND_READY) break;
    }
    if (n == poll_limit) return A04_MSDC_COMMAND_TIMEOUT;
    if (io->read32(io->context, RESPONSE0) & CARD_R1_ERRORS)
        return A04_MSDC_CARD_ERROR;
    for (n = 0; n < poll_limit; ++n) {
        bits = io->read32(io->context, INT_STATUS) & DATA_BITS;
        if (bits) io->write32(io->context, INT_STATUS, bits);
        if (bits & DATA_TIMEOUT) return A04_MSDC_DATA_TIMEOUT;
        if (bits & DATA_CRC) return A04_MSDC_DATA_CRC;
        if (bits & TRANSFER_COMPLETE) done = 1;
        count = io->read32(io->context, FIFO_STATUS) & 0xffu;
        while (count && offset < 512) {
            if (count >= 4 && 512 - offset >= 4) {
                val = io->read32(io->context, RX_DATA);
                buffer[offset++] = (uint8_t)val;
                buffer[offset++] = (uint8_t)(val >> 8);
                buffer[offset++] = (uint8_t)(val >> 16);
                buffer[offset++] = (uint8_t)(val >> 24);
                count -= 4;
            } else {
                buffer[offset++] = io->read8(io->context, RX_DATA);
                --count;
            }
        }
        if (done && offset == 512) return A04_MSDC_OK;
    }
    return done ? A04_MSDC_DATA_ERROR : A04_MSDC_DATA_TIMEOUT;
}
