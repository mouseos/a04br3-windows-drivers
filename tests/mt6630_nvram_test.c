/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nvram_sector.h"
static uint8_t disk[A04_NVRAM_BLOCK*3];
static void le32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int readpart(void *ctx,uint32_t off,void *out,unsigned n)
{(void)ctx;if(off>sizeof(disk)||n>sizeof(disk)-off)return 9;memcpy(out,disk+off,n);return 0;}
static int readsector(void *ctx,uint64_t lba,uint8_t out[512])
{(void)ctx;if(lba<8 || lba>=8+sizeof(disk)/512)return 9;memcpy(out,disk+(lba-8)*512,512);return 0;}
static void make_fixture(void)
{
 uint8_t *map=disk,*d=disk+A04_NVRAM_BLOCK,*bitmap=disk+2*A04_NVRAM_BLOCK;uint32_t sum=0;unsigned c=0;
 memset(disk,0,sizeof(disk));le32(bitmap+9,0xaabbccdd);le32(map,192);le32(map+4,514);le32(map+8,0xffffffffu);
 le32(map+64,7);le32(map+68,0);le32(map+72,514);memcpy(map+76,"/data/nvram/APCFG/APRDEB/WIFI",30);
 d[4]=2;d[5]=0x10;d[6]=0x20;d[7]=0x30;d[8]=0x40;d[9]=0x50;d[196]=1;
 for(unsigned i=0;i<512;i++)c=((i&1)?c^d[i]:c+d[i])&255;
 d[512]=0xaa;d[513]=(uint8_t)c;
 for(unsigned i=0;i<512;i+=4){uint32_t v=a04_nvram_le32(d+i);sum=(i&4)?sum+v:sum^v;}
 sum+=(uint32_t)d[512]|(uint32_t)d[513]<<8;le32(map+12,sum^514u);
}
int main(void)
{
 struct a04_nvram_reader r={0,sizeof(disk),readpart};struct a04_nvram_work w;uint8_t out[512];
 make_fixture();
 struct a04_nvram_sector_reader sectors;struct a04_nvram_reader sr;
 assert(!a04_nvram_sector_init(&sectors,&sr,0,readsector,8,sizeof(disk)/512,8+sizeof(disk)/512));
 assert(!a04_mt6630_calibration_read(&sr,&w,out));
 assert(a04_nvram_sector_init(&sectors,&sr,0,readsector,8,sizeof(disk)/512,7+sizeof(disk)/512));
 assert(a04_nvram_sector_init(&sectors,&sr,0,readsector,UINT64_MAX,1,UINT64_MAX));
 assert(!a04_mt6630_calibration_read(&r,&w,out));assert(out[4]==2 && out[9]==0x50 && out[196]==1);
 disk[A04_NVRAM_BLOCK+5]^=1;assert(a04_mt6630_calibration_read(&r,&w,out)!=0);
 make_fixture();le32(disk,0xfffffffcu);assert(a04_mt6630_calibration_read(&r,&w,out)!=0);
 make_fixture();disk[2*A04_NVRAM_BLOCK+1]=1;assert(a04_mt6630_calibration_read(&r,&w,out)!=0);
 puts("Synthetic on-device NVRAM lookup and checksums: OK");return 0;
}
