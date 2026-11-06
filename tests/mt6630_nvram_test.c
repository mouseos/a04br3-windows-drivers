/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "mt6630_nvram_fixture.h"
int main(void)
{
 struct a04_nvram_reader r={0,sizeof(disk),readpart};struct a04_nvram_work w;uint8_t out[512];
 make_fixture();
 struct a04_nvram_sector_reader sectors;struct a04_nvram_reader sr;
 assert(!a04_nvram_sector_init(&sectors,&sr,0,readsector,8,sizeof(disk)/512,8+sizeof(disk)/512));
 assert(!a04_mt6630_calibration_read(&sr,&w,out));
 uint8_t cross[4];assert(!sr.read(sr.context,511,cross,sizeof(cross)));
 assert(!memcmp(cross,disk+511,sizeof(cross)));
 assert(sr.read(sr.context,(uint32_t)sizeof(disk)-1,cross,sizeof(cross)));
 fail_sector=1;assert(a04_mt6630_calibration_read(&sr,&w,out)!=0);fail_sector=0;
 assert(a04_nvram_sector_init(&sectors,&sr,0,readsector,8,sizeof(disk)/512,7+sizeof(disk)/512));
 assert(a04_nvram_sector_init(&sectors,&sr,0,readsector,UINT64_MAX,1,UINT64_MAX));
 assert(!a04_mt6630_calibration_read(&r,&w,out));assert(out[4]==2 && out[9]==0x50 && out[196]==1);
 disk[A04_NVRAM_BLOCK+5]^=1;assert(a04_mt6630_calibration_read(&r,&w,out)!=0);
 make_fixture();le32(disk,0xfffffffcu);assert(a04_mt6630_calibration_read(&r,&w,out)!=0);
 make_fixture();disk[2*A04_NVRAM_BLOCK+1]=1;assert(a04_mt6630_calibration_read(&r,&w,out)!=0);
 puts("Synthetic on-device NVRAM lookup and checksums: OK");return 0;
}
