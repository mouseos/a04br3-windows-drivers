/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#define A04_WIFI_WLAN_SHA256_BYTES {0x2e,0x4c,0xd3,0x04,0x7d,0xa9,0x94,0xc0,0x26,0xc3,0xc1,0xf4,0x72,0xf0,0x45,0xcb,0x24,0x3c,0x0e,0xc8,0xb0,0x61,0x1d,0x7c,0x55,0xb8,0x86,0x1f,0xd9,0x41,0x8b,0xab}
#include "../drivers/network/mt6630/windows_firmware.c"
static int short_read,denied,closes,allocations,transfers;static unsigned char blob[128];
int KeGetCurrentIrql(void){return 0;}
void RtlInitUnicodeString(PUNICODE_STRING n,PCWSTR s){n->value=s;}
NTSTATUS ZwCreateFile(HANDLE *h,ULONG a,OBJECT_ATTRIBUTES *o,IO_STATUS_BLOCK *i,void *z,ULONG f,ULONG share,ULONG disp,ULONG opts,void *ea,ULONG ealen)
{(void)a;(void)o;(void)i;(void)z;(void)f;(void)disp;(void)opts;(void)ea;(void)ealen;assert(share==FILE_SHARE_READ);if(denied)return -1;*h=(void *)1;return 0;}
NTSTATUS ZwQueryInformationFile(HANDLE h,IO_STATUS_BLOCK *i,void *p,ULONG n,int t)
{FILE_STANDARD_INFORMATION *f=p;(void)h;(void)i;(void)n;(void)t;f->EndOfFile.QuadPart=sizeof(blob);f->Directory=0;return 0;}
NTSTATUS ZwReadFile(HANDLE h,void *e,void *a,void *c,IO_STATUS_BLOCK *i,void *p,ULONG n,void *o,void *k)
{(void)h;(void)e;(void)a;(void)c;(void)o;(void)k;memcpy(p,blob,n);i->Information=short_read?n-1:n;return 0;}
NTSTATUS ZwClose(HANDLE h){(void)h;closes++;return 0;}
void *ExAllocatePoolWithTag(int pool,size_t n,ULONG tag){(void)pool;(void)tag;allocations++;return malloc(n);}
void ExFreePoolWithTag(void *p,ULONG tag){size_t i;(void)tag;for(i=0;i<sizeof(blob);i++)assert(!((unsigned char *)p)[i]);allocations--;free(p);}
int a04_wmt_bridge_boot_wmt(struct a04_wmt_bridge *b,const void *d,unsigned n,const uint8_t e[32],int (*h)(const uint8_t *,size_t,uint8_t[32]),const struct a04_wmt_startup_options *o)
{(void)b;(void)d;(void)n;(void)e;(void)h;(void)o;transfers++;return 0;}
int a04_wmt_bridge_boot_wlan(struct a04_wmt_bridge *b,const void *d,unsigned n,int (*h)(const uint8_t *,size_t,uint8_t[32]))
{(void)b;(void)d;(void)n;(void)h;transfers++;return 0;}
static unsigned observed_stage;static ULONG observed_raw;
static void trace(void *context,unsigned stage,unsigned phase,unsigned kind,ULONG raw,ULONG detail)
{(void)context;(void)detail;assert(phase==A04_CP_RESULT&&kind==A04_CP_NT);observed_stage=stage;observed_raw=raw;}
int main(void)
{
 struct snapshot s={0};struct a04_windows_fw_snapshot fw={0};static struct a04_wmt_bridge bridge;struct a04_wmt_startup_options options={0};uint8_t hash[32]={0};
 memset(blob,0x55,sizeof(blob));assert(!read_snapshot(L"test",sizeof(blob),&s));assert(s.length==sizeof(blob)&&allocations==1&&closes==1);release_snapshot(&s);assert(!allocations);
 short_read=1;assert(read_snapshot(L"test",sizeof(blob),&s)==STATUS_END_OF_FILE&&!s.bytes&&!allocations);short_read=0;
 denied=1;assert(a04_windows_firmware_prepare_traced(&fw,hash,trace,&fw)==-1);
 assert(observed_stage==A04_CP_FW_READ_WMT&&observed_raw==(ULONG)-1&&!fw.wmt&&!allocations);
 assert(read_snapshot(L"test",sizeof(blob),&s)!=0&&!s.bytes);denied=0;
 assert(read_snapshot(L"test",sizeof(blob)-1,&s)==STATUS_INVALID_BUFFER_SIZE&&!s.bytes);
 bridge.identity_ready=1;assert(a04_windows_firmware_boot(&bridge,hash,&options)!=0&&!transfers&&!allocations);
 assert(!a04_sha256(blob,sizeof(blob),hash));
 assert(!a04_windows_firmware_prepare(&fw,hash) && fw.wmt && fw.wlan && allocations==2);
 assert(a04_windows_firmware_prepare(&fw,hash) && allocations==2);
 {uint8_t digest[32];assert(wlan_digest_matches(fw.wlan,fw.wlan_length,digest));fw.wlan[0]^=1;assert(!wlan_digest_matches(fw.wlan,fw.wlan_length,digest));fw.wlan[0]^=1;}
 a04_windows_firmware_release(&fw);assert(!allocations && !fw.wmt && !fw.wlan);
 assert(a04_windows_firmware_boot(&bridge,hash,&options)!=0&&!transfers&&!allocations);
 puts("Windows immutable firmware snapshot and preflight refusal: OK");return 0;
}
