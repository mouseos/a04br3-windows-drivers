/* Non-operational PortCls adapter skeleton: no subdevices and no MMIO. */
#include <portcls.h>
extern "C" DRIVER_INITIALIZE DriverEntry;
DRIVER_ADD_DEVICE Mt8167AddDevice;
static NTSTATUS Mt8167StartDevice(PDEVICE_OBJECT device, PIRP irp,
                                   PRESOURCELIST resources)
{
    UNREFERENCED_PARAMETER(device);
    UNREFERENCED_PARAMETER(irp);
    UNREFERENCED_PARAMETER(resources);
    /* No ownership broker, power/codec path or DMA teardown exists yet. */
    return STATUS_DEVICE_NOT_READY;
}
NTSTATUS Mt8167AddDevice(PDRIVER_OBJECT driver, PDEVICE_OBJECT pdo)
{
    return PcAddAdapterDevice(driver, pdo, PCPFNSTARTDEVICE(Mt8167StartDevice),
                              1, 0);
}
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    return PcInitializeAdapterDriver(driver, path, Mt8167AddDevice);
}
