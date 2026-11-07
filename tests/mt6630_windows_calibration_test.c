/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/network/mt6630/windows_calibration.c"
int KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
static int reads,sends,fail_read,fail_send,fail_alloc;static UCHAR expected[512];
NTSTATUS a04_windows_nvram_read_traced(UCHAR data[512],a04_session_trace_fn cb,void *ctx)
{(void)cb;(void)ctx;reads++;memcpy(data,expected,512);return fail_read?-1:0;}
int a04_wmt_bridge_load_calibration_traced(struct a04_wmt_bridge *b,const uint8_t data[512],a04_calibration_progress cb,void *ctx)
{(void)b;(void)cb;(void)ctx;sends++;assert(!memcmp(data,expected,512));return fail_send;}
void *ExAllocatePoolWithTag(int pool,size_t size,ULONG tag)
{(void)pool;(void)tag;return fail_alloc?NULL:malloc(size);}
void ExFreePoolWithTag(void *p,ULONG tag)
{(void)tag;for(unsigned i=0;i<512;i++)assert(!((UCHAR *)p)[i]);free(p);}
int main(void)
{
 static struct a04_wmt_bridge b;memset(expected,0x5a,sizeof(expected));
 assert(a04_windows_calibration_load(&b)&&!reads&&!sends);b.wlan_ready=1;
 fail_alloc=1;assert(a04_windows_calibration_load(&b)&&!reads);fail_alloc=0;
 fail_read=1;assert(a04_windows_calibration_load(&b)&&reads==1&&!sends);fail_read=0;
 fail_send=1;assert(a04_windows_calibration_load(&b)&&reads==2&&sends==1);fail_send=0;
 assert(!a04_windows_calibration_load(&b)&&reads==3&&sends==2);
 return 0;
}
