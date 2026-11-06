#include <cassert>
#include "wave_dma_live.h"
int probe_allocated=0,probe_flushes=0;
static DRIVER_LIST_CONTROL pending;static void *pendingCtx;static int puts,mode;static SCATTER_GATHER_LIST sg;
static void put(PDMA_ADAPTER,PSCATTER_GATHER_LIST p,BOOLEAN write){assert(p==&sg && write);puts++;}
static NTSTATUS get(PDMA_ADAPTER,PDEVICE_OBJECT dev,PMDL mdl,PVOID va,ULONG n,DRIVER_LIST_CONTROL cb,PVOID ctx,BOOLEAN write){assert(dev && mdl && va==mdl->address && n==8192 && write);if(mode==3)return STATUS_INSUFFICIENT_RESOURCES;if(mode==2){pending=cb;pendingCtx=ctx;return STATUS_SUCCESS;}cb(dev,nullptr,&sg,ctx);return STATUS_SUCCESS;}
int main(){DMA_OPERATIONS ops{get,put};_DMA_ADAPTER adapter{&ops};char x;MDL mdl{8192,0,&x};PDEVICE_OBJECT dev=&x;Mt8167WaveDma m{};
 sg.NumberOfElements=1;sg.Elements[0].Address.QuadPart=0x100000;sg.Elements[0].Length=8192;
 assert(!Mt8167WaveDmaMap(&adapter,dev,&mdl,8192,&m) && m.list==&sg && !puts && !probe_allocated && probe_flushes==1);
 m.exposed=TRUE;assert(Mt8167WaveDmaRelease(&m,FALSE)==STATUS_DEVICE_BUSY && m.list && !puts);
 assert(!Mt8167WaveDmaRelease(&m,TRUE) && !m.list && puts==1);
 assert(!Mt8167WaveDmaMap(&adapter,dev,&mdl,8192,&m) && !Mt8167WaveDmaRelease(&m,FALSE) && puts==2);
 sg.NumberOfElements=2;assert(Mt8167WaveDmaMap(&adapter,dev,&mdl,8192,&m)==STATUS_DEVICE_CONFIGURATION_ERROR && !m.list && puts==3);
 sg.NumberOfElements=1;sg.Elements[0].Address.QuadPart=0xfffff000LL;assert(Mt8167WaveDmaMap(&adapter,dev,&mdl,8192,&m)==STATUS_DEVICE_CONFIGURATION_ERROR && puts==4);
 sg.Elements[0].Address.QuadPart=0x100000;mode=3;assert(Mt8167WaveDmaMap(&adapter,dev,&mdl,8192,&m)==STATUS_INSUFFICIENT_RESOURCES && !probe_allocated);
 mode=2;assert(Mt8167WaveDmaMap(&adapter,dev,&mdl,8192,&m)==STATUS_IO_TIMEOUT && !m.list && probe_allocated==1 && puts==4);
 pending(dev,nullptr,&sg,pendingCtx);assert(!probe_allocated && puts==5);
}
