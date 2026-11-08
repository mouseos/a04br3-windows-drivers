#include <initguid.h>
#include "wavert_registration.h"
/* PcRegisterSubdevice retains Name for KS create lookup for device lifetime.
 * Never place these strings on StartDevice's stack (RT8.1 PortCls included). */
static WCHAR WaveName[]=L"Mt8167WaveRT";
static WCHAR TopologyName[]=L"Mt8167Topology";
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
#include "topology_miniport.h"
#endif
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
        status = PcRegisterSubdevice(device, WaveName, static_cast<PUNKNOWN>(port));
    }
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(NT_SUCCESS(status)) {
        PMINIPORTTOPOLOGY topology=NULL;PPORT topoPort=NULL;
        status=Mt8167NewTopology(&topology,device);
        if(NT_SUCCESS(status))status=PcNewPort(&topoPort,CLSID_PortTopology);
        if(NT_SUCCESS(status))
            status=topoPort->Init(device,irp,static_cast<PUNKNOWN>(topology),NULL,resources);
        if(NT_SUCCESS(status)) {
            status=PcRegisterSubdevice(device,TopologyName,static_cast<PUNKNOWN>(topoPort));
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
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
/* Caller supplies an actual capture backend with ADC/clock lease and RX DMA.
 * No endpoint is registered merely because descriptors are available. */
static WCHAR CaptureWaveName[]=L"Mt8167CaptureWaveRT";
static WCHAR CaptureTopologyName[]=L"Mt8167CaptureTopology";
NTSTATUS Mt8167RegisterCaptureWaveRt(PDEVICE_OBJECT device,PIRP irp,
    PRESOURCELIST resources,PMINIPORTWAVERT miniport)
{
    if(!device||!irp||!resources||!miniport)return STATUS_INVALID_PARAMETER;
    PPORT wave=NULL,topo=NULL;PMINIPORTTOPOLOGY topology=NULL;
    NTSTATUS s=PcNewPort(&wave,CLSID_PortWaveRT);
    if(NT_SUCCESS(s))s=wave->Init(device,irp,static_cast<PUNKNOWN>(miniport),NULL,resources);
    if(NT_SUCCESS(s))s=PcRegisterSubdevice(device,CaptureWaveName,static_cast<PUNKNOWN>(wave));
    if(NT_SUCCESS(s))s=Mt8167NewCaptureTopology(&topology);
    if(NT_SUCCESS(s))s=PcNewPort(&topo,CLSID_PortTopology);
    if(NT_SUCCESS(s))s=topo->Init(device,irp,static_cast<PUNKNOWN>(topology),NULL,resources);
    if(NT_SUCCESS(s))s=PcRegisterSubdevice(device,CaptureTopologyName,static_cast<PUNKNOWN>(topo));
    if(NT_SUCCESS(s))s=PcRegisterPhysicalConnection(device,static_cast<PUNKNOWN>(topo),1,static_cast<PUNKNOWN>(wave),1);
    if(topo)topo->Release();if(topology)topology->Release();if(wave)wave->Release();return s;
}
#endif
#endif
