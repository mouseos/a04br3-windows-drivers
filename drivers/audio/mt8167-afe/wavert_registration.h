#ifndef MT8167_WAVERT_REGISTRATION_H
#define MT8167_WAVERT_REGISTRATION_H
#include <portcls.h>
/* PortCls owns registration; the adapter retains references for explicit
 * reverse-order unregistration across PnP STOP/START. */
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
struct Mt8167RegisteredPorts {
 PPORT wave,topology;
 PUNREGISTERSUBDEVICE unregisterDevice;
 PUNREGISTERPHYSICALCONNECTION unregisterConnection;
 BOOLEAN waveRegistered,topologyRegistered,connected;
};
NTSTATUS Mt8167UnregisterWaveRt(PDEVICE_OBJECT,Mt8167RegisteredPorts *);
#endif
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
NTSTATUS Mt8167RegisterWaveRt(PDEVICE_OBJECT device, PIRP irp,
                              PRESOURCELIST resources, PMINIPORTWAVERT miniport
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
 ,Mt8167RegisteredPorts *registered
#endif
 );
#endif
#endif

#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
NTSTATUS Mt8167RegisterCaptureWaveRt(PDEVICE_OBJECT,PIRP,PRESOURCELIST,PMINIPORTWAVERT);
#endif
