/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "clock_snapshot.h"
/* CT3 MT8167 clk-mt8167.c ifr_muxes/plls and clkdbg_mt8167.c regnames.
 * No fmeter, hopping control, PMIC, DDRPHY or W1C registers are touched. */
int mt8167_clock_register(unsigned i, unsigned *bank, unsigned *offset)
{
    static const uint32_t offsets[MT8167_CLOCK_REGISTER_COUNT] =
        {0x000,0x014,0x100,0x104,0x110,0x120,0x124,0x130,0x140,0x144,0x150};
    if(i>=MT8167_CLOCK_REGISTER_COUNT || !bank || !offset)return -1;
    *bank=i?MT8167_CLOCK_APMIXED:MT8167_CLOCK_INFRA;*offset=offsets[i];return 0;
}
int mt8167_clock_snapshot_collect(const struct mt8167_clock_snapshot_io *io,
                                 struct mt8167_clock_snapshot *out)
{
    struct mt8167_clock_snapshot s={0};unsigned sweep,n,index;uint32_t value;
    int result=0;
    if(!out || !io || !io->read_index || !io->now_100ns)return -1;
    s.version=MT8167_CLOCK_SNAPSHOT_VERSION;s.bytes=sizeof(s);
    s.register_count=MT8167_CLOCK_REGISTER_COUNT;
    s.flags=MT8167_CLOCK_REFERENCE_UNKNOWN|MT8167_CLOCK_DDR_UNAVAILABLE|
            MT8167_CLOCK_EFFECTIVE_CPU_UNKNOWN;
    s.before_100ns=io->now_100ns(io->context);
    for(sweep=0;sweep<2;++sweep)for(n=0;n<MT8167_CLOCK_REGISTER_COUNT;++n){
        index=sweep?MT8167_CLOCK_REGISTER_COUNT-1u-n:n;
        if(io->read_index(io->context,index,&value) || value==UINT32_MAX){
            s.flags|=MT8167_CLOCK_READ_FAILED;result=-1;goto done;
        }
        if(sweep){s.second[index]=value;s.valid_second|=1u<<index;}
        else{s.first[index]=value;s.valid_first|=1u<<index;}
    }
    for(n=0;n<MT8167_CLOCK_REGISTER_COUNT;++n)
        if(s.first[n]!=s.second[n])s.flags|=MT8167_CLOCK_UNSTABLE;
done:
    s.after_100ns=io->now_100ns(io->context);
    if(s.after_100ns<s.before_100ns){s.flags|=MT8167_CLOCK_TIME_REVERSED;result=-1;}
    *out=s;return result;
}
