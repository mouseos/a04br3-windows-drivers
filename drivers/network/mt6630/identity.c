/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "identity.h"
int a04_mt6630_cis(const uint8_t *data,size_t size,struct a04_sdio_identity *out)
{
 size_t pos=0;unsigned found=0;uint16_t vendor=0,device=0;
 if(!out)return A04_CIS_MALFORMED;
 out->vendor=out->device=0;out->found=0;
 if(!data||!size)return A04_CIS_TRUNCATED;
 while(pos<size) {
  uint8_t code=data[pos++],len;
  if(code==0xff) {
   if(!found)return A04_CIS_ID_MISSING;
   out->vendor=vendor;out->device=device;out->found=1;
   return vendor==0x037a && device==0x6630 ? A04_CIS_OK : A04_CIS_MISMATCH;
  }
  if(code==0x00)continue;
  if(pos>=size)return A04_CIS_TRUNCATED;
  len=data[pos++];if(size-pos<len)return A04_CIS_TRUNCATED;
  if(code==0x20) {
   uint16_t v,d;if(len!=4||found)return A04_CIS_MALFORMED;
   v=(uint16_t)(data[pos]|((uint16_t)data[pos+1]<<8));
   d=(uint16_t)(data[pos+2]|((uint16_t)data[pos+3]<<8));
   vendor=v;device=d;found=1;
  }
  pos+=len;
 }
 return A04_CIS_TRUNCATED;
}
