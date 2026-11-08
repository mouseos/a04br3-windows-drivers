/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native 802.11 TX supplies a MAC header and an encapsulated frame body:
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/network/native-802-11-send-operations
 */
#include "native_tx.h"
#include "native_frame.h"
static int same(const uint8_t *a,const uint8_t *b,unsigned n)
{unsigned i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
int a04_native_tx_to_ethernet(const uint8_t *p,size_t n,const uint8_t ap[6],const uint8_t local[6],uint8_t *out,size_t cap,size_t *written)
{
 static const uint8_t snap[6]={0xaa,0xaa,3,0,0,0};size_t bytes,i;struct a04_native_frame f;uintptr_t in_addr,out_addr;
 if(!written)return -1;
 *written=0;
 /* Read only the owned snapshot; output may not overlap it. */
 if(!p||!ap||!local||!out||a04_native_frame_parse(p,n,1,&f)||(p[1]&0x7c)||!same(p+4,ap,6)||!same(p+10,local,6)||!same(p+f.header,snap,6))return -1;
 bytes=n-f.header-8+14;if(bytes>A04_NATIVE_ETH_MAX||cap<bytes)return -1;
 in_addr=(uintptr_t)p;out_addr=(uintptr_t)out;
 if(out_addr>=in_addr ? out_addr-in_addr<n : in_addr-out_addr<bytes)return -1;
 /* Input/output may not overlap; caller provides a distinct owned snapshot. */
 for(i=0;i<6;i++){out[i]=p[16+i];out[6+i]=p[10+i];}
 out[12]=p[f.header+6];out[13]=p[f.header+7];for(i=f.header+8;i<n;i++)out[14+i-f.header-8]=p[i];
 *written=bytes;return 0;
}
