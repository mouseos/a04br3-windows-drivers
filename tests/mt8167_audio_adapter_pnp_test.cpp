#include <cassert>
#include <cstring>
#include "pnp_backend.h"
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT,PUNICODE_STRING);
NTSTATUS Mt8167AddDevice(PDRIVER_OBJECT,PDEVICE_OBJECT);
struct Mt8167PnpBackend { Mt8167StreamBackend iface; };
static Mt8167PnpBackend object{};static int created,revoked,released,miniReleased,registerStatus,dispatches;static PCPFNSTARTDEVICE startDevice;
struct Mini : IMiniportWaveRT { ULONG Release() override {miniReleased++;return 0;} } mini;
NTSTATUS PcInitializeAdapterDriver(PDRIVER_OBJECT,PUNICODE_STRING,DRIVER_ADD_DEVICE*){return 0;}
NTSTATUS PcAddAdapterDevice(PDRIVER_OBJECT,PDEVICE_OBJECT,PCPFNSTARTDEVICE fn,ULONG,ULONG size){assert(size>64*sizeof(ULONG_PTR));startDevice=fn;return 0;}
NTSTATUS PcGetPhysicalDeviceObject(PDEVICE_OBJECT d,PDEVICE_OBJECT *p){*p=d;return 0;}
NTSTATUS PcDispatchIrp(PDEVICE_OBJECT,PIRP){dispatches++;return 0;}
NTSTATUS Mt8167PnpBackendCreate(PDEVICE_OBJECT,PRESOURCELIST,Mt8167PnpBackend **o){created++;*o=&object;return 0;}
const Mt8167StreamBackend *Mt8167PnpBackendInterface(Mt8167PnpBackend *o){return &o->iface;}
void Mt8167PnpBackendRevoke(Mt8167PnpBackend *){revoked++;}
void Mt8167PnpBackendRelease(Mt8167PnpBackend *){released++;}
NTSTATUS Mt8167NewMiniportWithBackend(PMINIPORTWAVERT *out,const Mt8167StreamBackend *p){assert(p==&object.iface);*out=&mini;return 0;}
NTSTATUS Mt8167RegisterWaveRt(PDEVICE_OBJECT,PIRP,PRESOURCELIST,PMINIPORTWAVERT){return registerStatus;}
struct Resources : IResourceList {
 _CM_PARTIAL_RESOURCE_DESCRIPTOR mem[2]{},irq{};
 Resources(){mem[0].u.Memory.Start.QuadPart=0x11140000;mem[0].u.Memory.Length=0x1000;mem[1].u.Memory.Start.QuadPart=0x11141000;mem[1].u.Memory.Length=0x9000;}
 ULONG NumberOfEntries() override {return 3;} ULONG NumberOfEntriesOfType(int t) override{return t==CmResourceTypeMemory?2:1;}
 PCM_PARTIAL_RESOURCE_DESCRIPTOR FindTranslatedEntry(int t,ULONG i) override{return t==CmResourceTypeMemory?&mem[i]:&irq;}
};
int main(){
 _DRIVER_OBJECT drv{};_UNICODE_STRING path{};_DEVICE_OBJECT dev{};_IRP irp{};ULONG_PTR ext[65]{};dev.DeviceExtension=ext;Resources res;
 assert(!DriverEntry(&drv,&path) && drv.MajorFunction[0]);assert(!Mt8167AddDevice(&drv,&dev) && startDevice);
 res.irq.Flags=CM_RESOURCE_INTERRUPT_LATCHED;assert(startDevice(&dev,&irp,&res)==STATUS_DEVICE_CONFIGURATION_ERROR && !created);res.irq.Flags=0;
 registerStatus=-8;assert(startDevice(&dev,&irp,&res)==-8 && created==1 && revoked==1 && released==1 && miniReleased==1);
 registerStatus=0;assert(!startDevice(&dev,&irp,&res) && created==2 && miniReleased==2);
 assert(startDevice(&dev,&irp,&res)==STATUS_DEVICE_BUSY);
 irp.stack.MinorFunction=IRP_MN_SURPRISE_REMOVAL;assert(!drv.MajorFunction[0](&dev,&irp));assert(revoked==2 && released==2 && dispatches==1);
 irp.stack.MinorFunction=IRP_MN_REMOVE_DEVICE;assert(!drv.MajorFunction[0](&dev,&irp));assert(revoked==2 && released==2 && dispatches==2);
}
