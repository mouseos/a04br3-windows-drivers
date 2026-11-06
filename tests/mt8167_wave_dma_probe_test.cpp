#include <cassert>
#include "wave_dma_probe.h"
int probe_allocated=0;
static DRIVER_LIST_CONTROL pending;static void *pendingCtx;static int puts,mode;static SCATTER_GATHER_LIST sg;
static void put(PDMA_ADAPTER,PSCATTER_GATHER_LIST p,BOOLEAN write){assert(p==&sg && write);puts++;}
static NTSTATUS get(PDMA_ADAPTER,PDEVICE_OBJECT dev,PMDL mdl,PVOID va,ULONG n,DRIVER_LIST_CONTROL cb,PVOID ctx,BOOLEAN write){
 assert(dev && mdl && va==mdl->address && n==8192 && write);if(mode==3)return STATUS_INSUFFICIENT_RESOURCES;
 if(mode==2){pending=cb;pendingCtx=ctx;return STATUS_SUCCESS;}cb(dev,nullptr,&sg,ctx);return STATUS_SUCCESS;}
int main(){DMA_OPERATIONS ops{get,put};_DMA_ADAPTER adapter{&ops};char x;MDL mdl{8192,0,&x};PDEVICE_OBJECT dev=&x;
 sg.NumberOfElements=1;sg.Elements[0].Address.QuadPart=0x100000;sg.Elements[0].Length=8192;
 assert(!Mt8167ProbeWaveDma(&adapter,dev,&mdl,8192) && puts==1 && !probe_allocated);
 sg.NumberOfElements=2;assert(Mt8167ProbeWaveDma(&adapter,dev,&mdl,8192)==STATUS_DEVICE_CONFIGURATION_ERROR && puts==2 && !probe_allocated);
 sg.NumberOfElements=1;sg.Elements[0].Address.QuadPart=0xfffff000LL;assert(Mt8167ProbeWaveDma(&adapter,dev,&mdl,8192)==STATUS_DEVICE_CONFIGURATION_ERROR && puts==3);
 sg.Elements[0].Address.QuadPart=0x100000;mode=3;assert(Mt8167ProbeWaveDma(&adapter,dev,&mdl,8192)==STATUS_INSUFFICIENT_RESOURCES && !probe_allocated);
 mode=2;assert(Mt8167ProbeWaveDma(&adapter,dev,&mdl,8192)==STATUS_IO_TIMEOUT && probe_allocated==1 && puts==3);
 pending(dev,nullptr,&sg,pendingCtx);assert(!probe_allocated && puts==4);
 mdl.offset=1;assert(Mt8167ProbeWaveDma(&adapter,dev,&mdl,8192)==STATUS_INVALID_PARAMETER && !probe_allocated);
}
