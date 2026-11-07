#include <initguid.h>
#include "wavert_registration.h"
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#include "topology_miniport.h"
#endif
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
/* PortCls borrows reference names for the full device lifetime. */
static WCHAR WaveReferenceName[]=L"Mt8167WaveRT";
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
static WCHAR TopologyReferenceName[]=L"Mt8167Topology";
#endif
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
        status = PcRegisterSubdevice(device, WaveReferenceName, static_cast<PUNKNOWN>(port));
    }
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(NT_SUCCESS(status)) {
        PMINIPORTTOPOLOGY topology=NULL;PPORT topoPort=NULL;
        status=Mt8167NewTopology(&topology);
        if(NT_SUCCESS(status))status=PcNewPort(&topoPort,CLSID_PortTopology);
        if(NT_SUCCESS(status))
            status=topoPort->Init(device,irp,static_cast<PUNKNOWN>(topology),NULL,resources);
        if(NT_SUCCESS(status)) {
            status=PcRegisterSubdevice(device,TopologyReferenceName,static_cast<PUNKNOWN>(topoPort));
        }
        if(NT_SUCCESS(status))
            status=PcRegisterPhysicalConnection(device,static_cast<PUNKNOWN>(port),1,
                static_cast<PUNKNOWN>(topoPort),0);
        if(topoPort)topoPort->Release();
        if(topology)topology->Release();
        /* PortCls owns successfully registered subdevices and cleans them
         * up on failed StartDevice/PnP removal. */
    }
#endif
    port->Release();
    return status;
}
#endif
