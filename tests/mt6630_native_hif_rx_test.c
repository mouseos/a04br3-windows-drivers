/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_hif_rx.h"
NDIS_STATUS a04_native_indicate_mpdu(NDIS_HANDLE a,PNET_BUFFER_LIST n,const uint8_t *p,ULONG l,const struct a04_native_rx_info *i)
{(void)a;(void)n;(void)p;(void)l;(void)i;return NDIS_STATUS_SUCCESS;}
static int rate(void *c,unsigned mode,unsigned raw,ULONG *phy,UCHAR *idx)
{(void)c;if(mode!=1||raw!=11)return -1;*phy=2;*idx=4;return 0;}
int main(void)
{
 uint8_t local[6]={2,1,2,3,4,5},ap[6]={2,6,7,8,9,10},p[64]={0};struct a04_native_hif_rx out={0};
 p[0]=64;p[2]=0;p[3]=0x48;p[5]=6;p[6]=24;p[16]=11;p[17]=0x10;p[25]=124;p[40]=8;p[41]=2;memcpy(p+44,local,6);memcpy(p+50,ap,6);
 assert(!a04_native_hif_rx_decode(p,sizeof(p),1u<<6,local,ap,NULL,rate,&out));
 assert(out.mpdu==p+40 && out.length==24 && out.info.frequency==2437 && out.info.rssi==-48 && out.info.phy==2 && out.info.rate==4);
 p[6]|=0x80;assert(a04_native_hif_rx_decode(p,sizeof(p),1u<<6,local,ap,NULL,rate,&out)==NDIS_STATUS_NOT_SUPPORTED);p[6]=24;
 p[25]=255;assert(a04_native_hif_rx_decode(p,sizeof(p),1u<<6,local,ap,NULL,rate,&out)==NDIS_STATUS_NOT_SUPPORTED);
 puts("Linux HIF raw MPDU and observed OOB decode: OK");return 0;
}
