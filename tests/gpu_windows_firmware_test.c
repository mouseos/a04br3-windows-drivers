/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdio.h>
#include <stdlib.h>
#include "windows_firmware.h"
#include "pvr_rogue_fwif.h"
static struct a04_gpu_firmware_state state;
static unsigned allocations, releases, signal_start;
static void *allocate(PDMA_ADAPTER adapter,ULONG size,PHYSICAL_ADDRESS *pa,BOOLEAN cache)
{
    (void)adapter; (void)cache;
    pa->QuadPart=allocations++ ? 0x90000000 : 0x80000000;
    return malloc(size);
}
static void release(PDMA_ADAPTER a,ULONG size,PHYSICAL_ADDRESS pa,void *memory,BOOLEAN cache)
{ (void)a; (void)size; (void)pa; (void)cache; ++releases; free(memory); }
unsigned KeGetCurrentIrql(void) { return PASSIVE_LEVEL; }
void KeStallExecutionProcessor(unsigned delay) { (void)delay; }
NTSTATUS KeDelayExecutionThread(int mode,BOOLEAN alertable,const LARGE_INTEGER *interval)
{
    (void)mode; (void)alertable; (void)interval;
    if(signal_start) {
        struct rogue_fwif_sysinit *sys=(void *)((uint8_t *)state.memory.heap+state.runtime.system_init);
        sys->firmware_started=1;
    }
    return STATUS_SUCCESS;
}
int main(int argc,char **argv)
{
    FILE *f; long n; uint8_t *image,*regs; NTSTATUS status;
    DMA_OPERATIONS ops={allocate,release}; struct fake_adapter adapter={&ops};
    if(argc!=2 || !(f=fopen(argv[1],"rb"))) return 1;
    if(fseek(f,0,SEEK_END) || (n=ftell(f))<=0 || fseek(f,0,SEEK_SET)) return 2;
    image=malloc((size_t)n); regs=calloc(1,0x80000);
    if(!image || !regs || fread(image,1,(size_t)n,f)!=(size_t)n) return 3;
    fclose(f);
    status=a04_gpu_firmware_boot(&state,&adapter,regs,0x80000,image,(size_t)n,A04_PVR_BVNC,FALSE,26000000);
    if(status!=STATUS_NOT_SUPPORTED || allocations || state.processor_released) return 4;
    signal_start=1;
    status=a04_gpu_firmware_boot(&state,&adapter,regs,0x80000,image,(size_t)n,A04_PVR_BVNC,TRUE,26000000);
    if(status!=STATUS_SUCCESS || !state.processor_released || !state.firmware_started || allocations!=2 || releases) return 5;
    /* Test process tears down its fake device only after the fake GPU is stopped. */
    a04_gpu_free_memory(&state.memory); memset(&state,0,sizeof(state)); allocations=releases=0; signal_start=0;
    status=a04_gpu_firmware_boot(&state,&adapter,regs,0x80000,image,(size_t)n,A04_PVR_BVNC,TRUE,26000000);
    if(status!=STATUS_IO_TIMEOUT || !state.processor_released || state.firmware_started || allocations!=2 || releases) return 6;
    a04_gpu_free_memory(&state.memory); free(regs); free(image);
    puts("Windows GPU boot composition and timeout lifetime checks passed");
    return 0;
}
