#include <initguid.h>
#include "wavert_registration.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
NTSTATUS Mt8167RegisterWaveRt(PDEVICE_OBJECT device, PIRP irp,
                              PRESOURCELIST resources, PMINIPORTWAVERT miniport)
{
    PPORT port = NULL;
    NTSTATUS status;
    if (!device || !irp || !resources || !miniport) return STATUS_INVALID_PARAMETER;
    status = PcNewPort(&port, CLSID_PortWaveRT);
    if (!NT_SUCCESS(status)) return status;
    status = port->Init(device, irp, static_cast<PUNKNOWN>(miniport), NULL, resources);
    if (NT_SUCCESS(status)) {
        WCHAR name[] = L"Mt8167WaveRT";
        status = PcRegisterSubdevice(device, name, static_cast<PUNKNOWN>(port));
    }
    port->Release();
    return status;
}
#endif
