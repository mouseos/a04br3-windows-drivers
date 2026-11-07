/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "../drivers/network/mt6630/wdf_wifi_parent_probe.c"
static int query_result,missing,refs,acquires,releases;
static void reference(PVOID c){(void)c;refs++;}
static void dereference(PVOID c){(void)c;refs--;}
static NTSTATUS acquire(PVOID c,ULONG *g){(void)c;(void)g;acquires++;return 0;}
static int live(PVOID c,ULONG g){(void)c;(void)g;return 0;}
static int update(PVOID c,ULONG g,unsigned b,unsigned o,ULONG m,ULONG v){(void)c;(void)g;(void)b;(void)o;(void)m;(void)v;return -1;}
static int write_shared(PVOID c,ULONG g,unsigned b,unsigned o,ULONG v){(void)c;(void)g;(void)b;(void)o;(void)v;return -1;}
static NTSTATUS release(PVOID c,ULONG g){(void)c;(void)g;releases++;return 0;}
NTSTATUS WdfFdoQueryForInterface(WDFDEVICE d,const GUID *g,PINTERFACE p,USHORT size,USHORT ver,void *data)
{
 A04_SHARED_WIFI_INTERFACE *i=(A04_SHARED_WIFI_INTERFACE *)p;(void)d;(void)data;
 assert(!memcmp(g,&A04SharedWifiInterfaceGuid,sizeof(*g)) && size==sizeof(*i) && ver==A04_SHARED_WIFI_INTERFACE_VERSION);
 if(query_result)return query_result;
 i->Header.InterfaceReference=reference;i->Header.InterfaceDereference=dereference;i->Acquire=missing?NULL:acquire;i->ReleaseQuiescent=release;i->Live=live;i->UpdateShared=update;i->WriteShared=write_shared;refs++;return 0;
}
void WdfDeviceInitSetPnpPowerEventCallbacks(PWDFDEVICE_INIT i,WDF_PNPPOWER_EVENT_CALLBACKS *p){(void)i;(void)p;}
NTSTATUS WdfDeviceCreate(PWDFDEVICE_INIT *i,WDF_OBJECT_ATTRIBUTES *a,WDFDEVICE *d){(void)i;(void)a;(void)d;return 0;}
NTSTATUS WdfDriverCreate(PDRIVER_OBJECT d,PUNICODE_STRING p,void *a,WDF_DRIVER_CONFIG *c,void *h){(void)d;(void)p;(void)a;(void)c;(void)h;return 0;}
int main(void)
{
 query_result=STATUS_DEVICE_NOT_READY;assert(WifiProbePrepare(NULL,NULL,NULL)==STATUS_DEVICE_NOT_READY && !refs);
 query_result=0;missing=1;assert(WifiProbePrepare(NULL,NULL,NULL)==STATUS_DEVICE_CONFIGURATION_ERROR && !refs);
 missing=0;assert(WifiProbePrepare(NULL,NULL,NULL)==STATUS_DEVICE_NOT_READY && !refs && !acquires && !releases);
 return 0;
}
