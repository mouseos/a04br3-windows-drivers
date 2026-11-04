/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MTKW parsing and measured revision/digest policy adapted from local untracked
 * UniXNU-a04br3 package/wifi/sdio/A04BR3WLANFirmware.h, SHA-256
 * a584a97f244eaa99eba23baa9869c22338a00279ce9f764ef890bfc27b64471b.
 * No download here. */
#include "wlan_firmware.h"
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t crc32(const uint8_t *p,size_t n)
{ uint32_t v=0xffffffffu;size_t i;unsigned k;for(i=0;i<n;i++){v^=p[i];for(k=0;k<8;k++)v=(v>>1)^((v&1u)?0xedb88320u:0);}return v^0xffffffffu; }
int a04_wlan_firmware_stage(uint32_t chip,uint32_t hw,uint32_t rom,
 const uint8_t *b,size_t length,int (*sha256)(const uint8_t *,size_t,uint8_t[32]),struct a04_wlan_firmware *out)
{
 static const uint8_t expected[2][32]={
 {0xf2,0x8e,0x4c,0x9f,0x03,0x82,0x1c,0x1e,0xc6,0x72,0x9b,0x3b,0x9b,0x9c,0x05,0xbf,0x82,0x57,0x5b,0x5e,0xd5,0x8c,0xce,0x44,0x7e,0x65,0x35,0x06,0xf6,0xbd,0xdd,0x45},
 {0x07,0xfe,0x1a,0x22,0x5e,0x6b,0x9d,0xe6,0xb3,0x10,0x89,0x4d,0xb3,0xb4,0x0b,0xcc,0x1c,0xb6,0x61,0x07,0xb9,0x13,0xe4,0xc4,0x3e,0xb6,0xa6,0x20,0x89,0xaa,0xda,0x22}};
 struct a04_wlan_firmware tmp={0};uint8_t digest[32];unsigned i,j,n,candidate=0;
 if(!out)return -1;
 out->count=out->candidate=0;
 if(!b||!sha256||chip!=0x6630||hw!=0x8a12||rom!=0x8a11||length!=237008)return -1;
 if(sha256(b,length,digest))return -1;
 for(i=0;i<2;i++){unsigned different=0;for(j=0;j<32;j++)different|=digest[j]^expected[i][j];if(!different)candidate=i+1;}
 if(!candidate || le32(b)!=0x574b544du || le32(b+12) || crc32(b+8,length-8)!=le32(b+4))return -1;
 n=le32(b+8);if(!n||n>16||length<16u+n*16u)return -1;
 for(i=0;i<n;i++){
  const uint8_t *s=b+16u+16u*i;uint32_t off=le32(s),len=le32(s+8),addr=le32(s+12);
  if(le32(s+4)||off<16u+n*16u||off>length||!len||len>length-off||len-1>0xffffffffu-addr)return -1;
  for(j=0;j<i;j++){size_t po=(size_t)(tmp.section[j].data-b);uint32_t pn=tmp.section[j].length,pa=tmp.section[j].address;
   if(((uint64_t)off<po+pn && po<(uint64_t)off+len)||((uint64_t)addr<(uint64_t)pa+pn && (uint64_t)pa<(uint64_t)addr+len))return -1;}
  tmp.section[i].data=b+off;tmp.section[i].address=addr;tmp.section[i].length=len;
 }
 tmp.count=n;tmp.candidate=candidate;*out=tmp;return 0;
}
