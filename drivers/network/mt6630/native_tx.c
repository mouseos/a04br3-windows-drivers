/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native 802.11 TX supplies a MAC header and an encapsulated frame body:
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/network/native-802-11-send-operations
 */
#include "native_tx.h"
#include "native_limits.h"
static int same(const uint8_t *a,const uint8_t *b,unsigned n)
{unsigned i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
int a04_native_tx_to_ethernet(const uint8_t *p,size_t n,const uint8_t ap[6],const uint8_t local[6],uint8_t *out,size_t cap,size_t *written)
{
 static const uint8_t snap[6]={0xaa,0xaa,3,0,0,0};size_t bytes,i;uintptr_t in_addr,out_addr;
 if(!written)return -1;
 *written=0;
 /* Exact data subtype, ToDS only; no retry, protected, fragments or QoS. */
 if(!p||!ap||!local||!out||n<32||n>A04_NATIVE_MPDU_MAX||p[0]!=0x08||p[1]!=0x01||
    (p[22]&15)||!same(p+4,ap,6)||!same(p+10,local,6)||!same(p+24,snap,6))return -1;
 bytes=n-18;if(bytes>A04_NATIVE_ETH_MAX||cap<bytes)return -1;
 in_addr=(uintptr_t)p;out_addr=(uintptr_t)out;
 if(out_addr>=in_addr ? out_addr-in_addr<n : in_addr-out_addr<bytes)return -1;
 /* Input/output may not overlap; caller provides a distinct owned snapshot. */
 for(i=0;i<6;i++){out[i]=p[16+i];out[6+i]=p[10+i];}
 out[12]=p[30];out[13]=p[31];for(i=32;i<n;i++)out[14+i-32]=p[i];
 *written=bytes;return 0;
}
