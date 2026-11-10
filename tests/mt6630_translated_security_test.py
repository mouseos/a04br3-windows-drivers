#!/usr/bin/env python3
"""実RX security関数と実HIF decoderのhost境界検証。NDIS/実機通信の証明ではない。"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'drivers/network/mt6630/native_security.c').read_text()

def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

body = '\n'.join(function(s) for s in (
    'static int same(', 'static int cipher_active(', 'static unsigned exemption(',
    'NDIS_STATUS a04_os_security_receive('))
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include "native_hif_rx.h"
#include "native_limits.h"
#define DOT11_AUTH_ALGO_RSNA_PSK 7
#define DOT11_CIPHER_ALGO_CCMP 4
#define DOT11_EXEMPT_NO_EXEMPTION 0
#define DOT11_EXEMPT_ALWAYS 1
#define DOT11_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE 2
#define DOT11_EXEMPT_UNICAST 1
#define DOT11_EXEMPT_MULTICAST 2
#define DOT11_EXEMPT_BOTH 3
typedef struct {USHORT usEtherType,usExemptionActionType,usExemptionPacketType;} DOT11_PRIVACY_EXEMPTION;
#include "native_security.h"
struct bridge {struct {unsigned permitted;} regulatory;};
struct a04_ndis_runtime {struct a04_os_security os_security;
 struct {struct bridge *bridge;struct {struct {uint8_t mac[6];unsigned wlan;} local;struct {uint8_t mac[6];} ap;} station;} radio;
 uint8_t rx_bytes[A04_NATIVE_MPDU_MAX];};
static unsigned a04_wlan_be16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
int a04_ndis_rate_lookup(void *c,unsigned mode,unsigned raw,ULONG *phy,UCHAR *rate)
{(void)c;if(mode!=1||raw!=11)return -1;*phy=0;*rate=12;return 0;}
NDIS_STATUS a04_native_indicate_mpdu(NDIS_HANDLE a,PNET_BUFFER_LIST n,const uint8_t *p,ULONG l,const struct a04_native_rx_info *i)
{(void)a;(void)n;(void)p;(void)l;(void)i;assert(0);return 0;}
''' + body + r'''
int main(void)
{
 struct bridge b={{1u<<6}};struct a04_ndis_runtime a={0};struct a04_native_hif_rx rx;
 uint8_t p[128]={0};unsigned n=86;uint8_t local[]={2,1,2,3,4,5},ap[]={2,6,7,8,9,10};
 a.radio.bridge=&b;memcpy(a.radio.station.local.mac,local,6);memcpy(a.radio.station.ap.mac,ap,6);a.radio.station.local.wlan=3;
 a.os_security.auth=7;a.os_security.unicast=a.os_security.multicast=4;
 /* groups4/1/3; actual CCMP PN9, unicast WTBL3, raw group4 FromDS. */
 p[0]=n;p[3]=0x5a;p[5]=6;p[6]=0x8e;p[8]=3;p[9]=0x40;
 p[16]=8;p[17]=0x42;memcpy(p+18,ap,6);p[24]=0x10;p[32]=9;
 p[48]=11;p[49]=0x10;p[57]=124;memcpy(p+72,local,6);memcpy(p+78,local,6);p[84]=8;
 assert(a04_os_security_receive(&a,p,n,&rx)); /* no installed PTK */
 a.os_security.ptk.valid=1;a.os_security.ptk.wlan=3;
 assert(a04_os_security_receive(&a,p,n,&rx));assert(!a.os_security.ptk.pn[0]); /* closed port */
 a.os_security.port_open=1;assert(!a04_os_security_receive(&a,p,n,&rx));assert(a.os_security.ptk.pn[0]==9);
 assert(a04_os_security_receive(&a,p,n,&rx)); /* repeated PN */
 p[32]=10;p[8]=4;assert(a04_os_security_receive(&a,p,n,&rx));assert(a.os_security.ptk.pn[0]==9);p[8]=3;
 assert(!a04_os_security_receive(&a,p,n,&rx));
 /* Group-key slot/WTBL/PN remain independent of unicast. */
 memset(p+72,255,6);p[4]=2<<6;p[8]=19;p[32]=1;
 assert(a04_os_security_receive(&a,p,n,&rx));a.os_security.gtk[2].valid=1;a.os_security.gtk[2].wlan=19;
 assert(!a04_os_security_receive(&a,p,n,&rx));assert(a.os_security.gtk[2].pn[0]==1&&a.os_security.ptk.pn[0]==10);
 p[10]=0x10;assert(a04_os_security_receive(&a,p,n,&rx));p[10]=0;
 /* Real unencrypted EAPOL still needs the OS privacy exemption and AP SA. */
 memset(p,0,sizeof(p));n=70;p[0]=n;p[3]=0x58;p[5]=6;p[6]=0x8e;p[8]=3;
 p[16]=8;p[17]=2;memcpy(p+18,ap,6);p[32]=11;p[33]=0x10;p[41]=124;
 memcpy(p+56,local,6);memcpy(p+62,ap,6);p[68]=0x88;p[69]=0x8e;
 a.os_security.port_open=0;assert(a04_os_security_receive(&a,p,n,&rx));
 a.os_security.exemption_count=1;a.os_security.exemptions[0]=(DOT11_PRIVACY_EXEMPTION){0x888e,1,3};
 assert(!a04_os_security_receive(&a,p,n,&rx));p[62]^=1;assert(a04_os_security_receive(&a,p,n,&rx));
 puts("PASS actual security RX: translated PTK/GTK, WTBL, PN, closed port, ICV, AP-only EAPOL exemptions; no NDIS/hardware execution");
}
'''
out = root / 'build/mt6630-translated-security'
out.mkdir(parents=True, exist_ok=True)
(out / 'boundary.c').write_text(fixture)
subprocess.run(['clang', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
                '-Wall', '-Wextra', '-Werror', '-Wno-logical-op-parentheses',
                '-I' + str(root / 'tests/native_scan_stubs'),
                '-I' + str(root / 'drivers/network/mt6630'),
                str(out / 'boundary.c'), str(root / 'drivers/network/mt6630/native_hif_rx.c'),
                '-o', str(out / 'boundary-test')], check=True)
subprocess.run([str(out / 'boundary-test')], check=True)
