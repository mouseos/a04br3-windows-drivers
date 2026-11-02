/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include "cdb.h"

int main(void)
{
    struct a04_block_request r = {0};
    uint8_t read10[10] = {0x28,0,0,0,0,9,0,0,2,0};
    uint8_t write16[16] = {0x8a,0,0,0,0,0,0,0,0,9,0,0,0,2,0,0};
    assert(a04_decode_block_cdb(read10, 10, 11, &r) == A04_CDB_OK);
    assert(r.lba == 9 && r.block_count == 2 && !r.write);
    assert(a04_decode_block_cdb(read10, 10, 10, &r) == A04_CDB_OUT_OF_RANGE);
    assert(a04_decode_block_cdb(read10, 9, 11, &r) == A04_CDB_INVALID);
    assert(a04_decode_block_cdb(write16, 16, 11, &r) == A04_CDB_OK);
    assert(r.lba == 9 && r.block_count == 2 && r.write);
    write16[2] = 1;
    assert(a04_decode_block_cdb(write16, 16, UINT64_MAX, &r) == A04_CDB_OUT_OF_RANGE);
    write16[2] = 0; write16[6] = 0xff; write16[7] = 0xff;
    write16[8] = 0xff; write16[9] = 0xff;
    assert(a04_decode_block_cdb(write16, 16, UINT64_MAX, &r) == A04_CDB_OUT_OF_RANGE);
    read10[7] = read10[8] = 0;
    assert(a04_decode_block_cdb(read10, 10, 9, &r) == A04_CDB_OK && r.block_count == 0);
    read10[0] = 0xff;
    assert(a04_decode_block_cdb(read10, 10, 11, &r) == A04_CDB_UNSUPPORTED);
    assert(a04_decode_block_cdb(0, 10, 11, &r) == A04_CDB_INVALID);
    assert(a04_decode_block_cdb(read10, 10, 11, 0) == A04_CDB_INVALID);
    return 0;
}
