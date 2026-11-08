/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_hif_rx.h"
#include "native_frame.h"
#include "native_retry.h"
#include "native_tx.h"
NDIS_STATUS a04_native_indicate_mpdu(NDIS_HANDLE a,PNET_BUFFER_LIST n,const uint8_t *p,ULONG l,const struct a04_native_rx_info *i)
{(void)a;(void)n;(void)p;(void)l;(void)i;return NDIS_STATUS_SUCCESS;}
static int rate(void *c,unsigned mode,unsigned raw,ULONG *phy,UCHAR *idx)
{(void)c;if(mode!=1||raw!=11)return -1;*phy=2;*idx=4;return 0;}
int main(void)
{
 uint8_t local[6]={2,1,2,3,4,5},ap[6]={2,6,7,8,9,10};
 uint8_t p[4096],before[4096],out[A04_NATIVE_MPDU_MAX],eth[A04_NATIVE_ETH_MAX];struct a04_native_hif_rx rx;
 for(unsigned h=24;h<=30;h+=2){if(h==28)continue;
 for(unsigned tid=0;tid<8;tid++){
  unsigned n=40+h+8+A04_NATIVE_MTU;memset(p,0,sizeof(p));p[0]=n;p[1]=n>>8;p[3]=0x48;p[5]=6;p[6]=h;p[9]=h==24?0:tid;
  p[16]=11;p[17]=0x10;p[25]=124;p[40]=h==24?8:0x88;p[41]=h==30?0x82:2;
  memcpy(p+44,local,6);memcpy(p+50,ap,6);p[64]=tid;
  memcpy(p+40+h,"\xaa\xaa\x03\0\0\0\x08\0",8);p[n-1]=0xf5;
  memcpy(before,p,n);
  assert(!a04_native_hif_rx_decode(p,n,1u<<6,local,ap,NULL,rate,&rx));
  assert(rx.header==h&&rx.tid==(h==24?0:tid)&&rx.info.priority==rx.tid);
  assert(rx.mpdu==p+40&&rx.length==h+8+A04_NATIVE_MTU&&rx.info.rssi==-48);
  {struct a04_native_retry owner={0};struct a04_native_retry_ticket ticket;p[41]|=8;
   assert(!a04_native_hif_rx_decode(p,n,1u<<6,local,ap,NULL,rate,&rx));
   assert(!a04_native_retry_check(&owner,1,rx.mpdu,rx.length,&ticket));
   a04_native_retry_commit(&owner,&ticket);assert(a04_native_retry_check(&owner,1,rx.mpdu,rx.length,&ticket)==1);
   p[41]&=~8u;}

  size_t normalized=a04_native_frame_copy(out,rx.mpdu,rx.length,h);
  assert(normalized==A04_NATIVE_MTU+32&&out[0]==8&&out[1]==2&&out[normalized-1]==0xf5&&!memcmp(p,before,n));
  for(unsigned bit=0;bit<3;bit++){unsigned flag=(unsigned[]){4,0x40,0x80}[bit];p[41]^=flag;assert(a04_native_hif_rx_decode(p,n,1u<<6,local,ap,NULL,rate,&rx));p[41]^=flag;}
  p[6]=h-1;assert(a04_native_hif_rx_decode(p,n,1u<<6,local,ap,NULL,rate,&rx));p[6]=h;
  if(h!=24){p[9]=tid^1;assert(a04_native_hif_rx_decode(p,n,1u<<6,local,ap,NULL,rate,&rx));p[9]=tid;
   p[64]|=0x80;assert(a04_native_hif_rx_decode(p,n,1u<<6,local,ap,NULL,rate,&rx));p[64]=tid;}
  for(unsigned cut=0;cut<h+8;cut++){struct a04_native_frame f;assert(a04_native_frame_parse(p+40,cut,2,&f));}
  /* Same LLC offset/TID on TX, independent snapshot and output. */
  p[41]=h==30?0x81:1;memcpy(p+44,ap,6);memcpy(p+50,local,6);memcpy(p+56,local,6);
  size_t bytes=0;assert(!a04_native_tx_to_ethernet(p+40,n-40,ap,local,eth,sizeof(eth),&bytes));
  assert(bytes==sizeof(eth)&&eth[12]==8&&eth[13]==0&&eth[bytes-1]==0xf5);
  assert(a04_native_tx_to_ethernet(p+40,n-40,ap,local,eth,bytes-1,&bytes)&&!bytes);
  assert(a04_native_tx_to_ethernet(p+40,n-40,ap,local,p+40,sizeof(p)-40,&bytes)&&!bytes);
 }}
 /* Protected QoS: group1 six-byte PN and TID validated before security. */
 memset(p,0,sizeof(p));unsigned n=56+26+8;p[0]=n;p[3]=0x4a;p[5]=6;p[6]=26;p[9]=0x46;p[16]=7;p[32]=11;p[33]=0x10;p[41]=124;
 p[56]=0x88;p[57]=0x42;memcpy(p+60,local,6);memcpy(p+66,ap,6);p[80]=6;memcpy(p+82,"\xaa\xaa\x03\0\0\0\x08\0",8);
 assert(!a04_native_hif_rx_decode_security(p,n,1u<<6,local,ap,NULL,rate,&rx));assert(rx.pn==7&&rx.tid==6&&rx.header==26);
 p[80]=5;assert(a04_native_hif_rx_decode_security(p,n,1u<<6,local,ap,NULL,rate,&rx));p[80]=6;
 p[10]=0x10;assert(a04_native_hif_rx_decode_security(p,n,1u<<6,local,ap,NULL,rate,&rx));p[10]=0;
 p[57]|=8;assert(!a04_native_hif_rx_decode_security(p,n,1u<<6,local,ap,NULL,rate,&rx)); /* Parsing a retry does not authorize its indication. */
 puts("24/26/30 MAC, TID0..7, HIF/LLC/CCMP bounds and immutable ownership: OK");return 0;
}
