/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wlan_firmware.h"
static uint8_t image[237008];
static const uint8_t hash[32]={0xf2,0x8e,0x4c,0x9f,0x03,0x82,0x1c,0x1e,0xc6,0x72,0x9b,0x3b,0x9b,0x9c,0x05,0xbf,0x82,0x57,0x5b,0x5e,0xd5,0x8c,0xce,0x44,0x7e,0x65,0x35,0x06,0xf6,0xbd,0xdd,0x45};
static int digest(const uint8_t *p,size_t n,uint8_t out[32]){(void)p;(void)n;memcpy(out,hash,32);return 0;}
static void put(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
int main(void)
{
 struct a04_wlan_firmware fw={0};put(image,0x574b544d);put(image+8,1);put(image+16,32);put(image+24,4);put(image+28,0x1000);put(image+4,crc(image+8,sizeof(image)-8));
 /* Simulated hash callback allows a synthetic container; not a signature test. */
 assert(!a04_wlan_firmware_stage(0x6630,0x8a12,0x8a11,image,sizeof(image),digest,&fw));assert(fw.count==1&&fw.section[0].length==4);
 assert(a04_wlan_firmware_stage(0x6630,0x8a11,0x8a11,image,sizeof(image),digest,&fw)&&!fw.count);
 image[32]^=1;assert(a04_wlan_firmware_stage(0x6630,0x8a12,0x8a11,image,sizeof(image),digest,&fw));
 puts("Synthetic WLAN image bounds, identity and CRC: OK");return 0;
}
