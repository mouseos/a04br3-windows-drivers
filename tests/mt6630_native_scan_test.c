/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "native_scan.h"
int main(void)
{
 union {DOT11_SCAN_REQUEST_V2 align;UCHAR bytes[256];} b;DOT11_SCAN_REQUEST_V2 *p=&b.align;
 struct a04_native_scan_request out;DOT11_SSID id={0};ULONG needed,base=(ULONG)offsetof(DOT11_SCAN_REQUEST_V2,ucBuffer);
 memset(&b,0,sizeof(b));p->dot11BSSType=dot11_BSS_type_any;memset(p->dot11BSSID,0xff,6);p->dot11ScanType=dot11_scan_type_active;
 p->uNumOfdot11SSIDs=1;p->udot11SSIDsOffset=1;id.uSSIDLength=3;memcpy(id.ucSSID,"abc",3);memcpy(b.bytes+base+1,&id,sizeof(id));
 p->uIEsOffset=1+sizeof(id);p->uIEsLength=4;b.bytes[base+p->uIEsOffset]=221;b.bytes[base+p->uIEsOffset+1]=2;
 assert(a04_native_scan_parse(b.bytes,base+p->uIEsOffset+4,&out,&needed)==NDIS_STATUS_SUCCESS);
 assert(!needed && out.ssid_length==3 && !memcmp(out.ssid,"abc",3) && out.ie_length==4);
 out.ssid_length=99;assert(a04_native_scan_parse(b.bytes,base+3,&out,&needed)==NDIS_STATUS_INVALID_LENGTH && out.ssid_length==99);
 p->udot11SSIDsOffset=0xfffffff0u;assert(a04_native_scan_parse(b.bytes,sizeof(b),&out,&needed)==NDIS_STATUS_INVALID_LENGTH);
 p->udot11SSIDsOffset=1;p->uIEsOffset=0xfffffff0u;assert(a04_native_scan_parse(b.bytes,sizeof(b),&out,&needed)==NDIS_STATUS_INVALID_LENGTH);
 p->uIEsOffset=1+sizeof(id);b.bytes[base+p->uIEsOffset+1]=5;assert(a04_native_scan_parse(b.bytes,sizeof(b),&out,&needed)==NDIS_STATUS_INVALID_DATA);
 p->uNumOfdot11SSIDs=2;assert(a04_native_scan_parse(b.bytes,sizeof(b),&out,&needed)==NDIS_STATUS_INVALID_LENGTH);
 puts("Native 802.11 scan request bounds and snapshot: OK");return 0;
}
