/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/wddm-display/mt8167_cmdq_packet.h"
int main(void)
{
    uint32_t words[8]={0},restore[8]={0};
    assert(!a04_mt8167_cmdq_l0_address(words,7,27,0x80001000));
    assert(!a04_mt8167_cmdq_l0_address(words,8,77,0x80001000));
    assert(!a04_mt8167_cmdq_l0_address(words,8,27,0));
    assert(!a04_mt8167_cmdq_l0_address(words,8,27,0x80001001));
    for(unsigned i=0;i<8;i++)assert(!words[i]);
    assert(a04_mt8167_cmdq_l0_address(words,8,27,0xb8c1b000));
    assert(words[0]==0x8001 && words[1]==0x2000001b);
    assert(words[2]==0xb8c1b000 && words[3]==0x04017f40);
    assert(words[4]==1 && words[5]==0x40000000 && words[6]==8 && words[7]==0x10000000);
    assert(a04_mt8167_cmdq_l0_address(restore,8,38,0x90000000));
    assert(restore[1]==0x20000026 && restore[2]==0x90000000 && restore[3]==words[3]);
    return 0;
}
