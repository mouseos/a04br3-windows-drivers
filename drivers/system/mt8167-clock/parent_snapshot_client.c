/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "parent_snapshot_client.h"
NTSTATUS Mt8167ReadParentClockSnapshot(WDFDEVICE device,
                                      struct mt8167_clock_snapshot *out)
{
    MT8167_CLOCK_SNAPSHOT_INTERFACE iface;NTSTATUS status;
    if(!device || !out)return STATUS_INVALID_PARAMETER;
    if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    RtlZeroMemory(&iface,sizeof(iface));RtlZeroMemory(out,sizeof(*out));
    status=WdfFdoQueryForInterface(device,&Mt8167ClockSnapshotInterfaceGuid,
            (PINTERFACE)&iface,sizeof(iface),MT8167_CLOCK_INTERFACE_VERSION,NULL);
    if(!NT_SUCCESS(status))return status;
    if(iface.Header.Size!=sizeof(iface) ||
       iface.Header.Version!=MT8167_CLOCK_INTERFACE_VERSION ||
       iface.ParentResourceContract!=MT8167_CLOCK_PARENT_RESOURCE_CONTRACT ||
       !iface.Header.Context || !iface.Header.InterfaceReference ||
       !iface.Header.InterfaceDereference || !iface.Snapshot)
        status=STATUS_REVISION_MISMATCH;
    else status=iface.Snapshot(iface.Header.Context,sizeof(*out),out);
    /* WdfFdoQueryForInterface already acquired the provider's reference. */
    if(iface.Header.InterfaceDereference)
        iface.Header.InterfaceDereference(iface.Header.Context);
    return status;
}
