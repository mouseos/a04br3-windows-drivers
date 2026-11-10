/******************************Module*Header*******************************\
* Module Name: bdd.cxx
*
* Basic Display Driver functions implementation
*
*
* Copyright (c) 2010 Microsoft Corporation
\**************************************************************************/


#include "BDD.hxx"
#include "present_bounds.h"
#include "ge8300_trace.hxx"
#include "../start_gap359.h"

#pragma code_seg("PAGE")

/* PresentDisplayOnly has WDDM level-zero synchronization and can be entered
 * concurrently. A kernel mutex leaves special APCs enabled for Zw I/O. */
class A04PresentGuard
{
    KMUTEX* m_Mutex;
public:
    explicit A04PresentGuard(KMUTEX* mutex) : m_Mutex(mutex) {}
    ~A04PresentGuard() { KeReleaseMutex(m_Mutex,FALSE); }
};

BASIC_DISPLAY_DRIVER::BASIC_DISPLAY_DRIVER(_In_ DEVICE_OBJECT* pPhysicalDeviceObject) : m_pPhysicalDevice(pPhysicalDeviceObject),
                                                                                        m_MonitorPowerState(PowerDeviceD0),
                                                                                        m_AdapterPowerState(PowerDeviceD0)
{
    PAGED_CODE();
    *((UINT*)&m_Flags) = 0;
    m_Flags._LastFlag = TRUE;
    RtlZeroMemory(&m_DxgkInterface, sizeof(m_DxgkInterface));
    RtlZeroMemory(&m_StartInfo, sizeof(m_StartInfo));
    RtlZeroMemory(m_CurrentModes, sizeof(m_CurrentModes));
    RtlZeroMemory(&m_DeviceInfo, sizeof(m_DeviceInfo));
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
    RtlZeroMemory(&m_DisplayResources, sizeof(m_DisplayResources));
#endif
    KeInitializeMutex(&m_PresentMutex,0);
#if A04_MT8167_DISPLAY_BRIGHTNESS
    m_Brightness.Initialize();m_UnifiedBrightnessResources=NULL;
#endif
#if A04_GE8300_WDDM_SHARED_AUTOSTART
    KeInitializeEvent(&m_SharedWorkerStop,NotificationEvent,FALSE);
    KeInitializeMutex(&m_SharedWorkerStopMutex,0);
    m_SharedWorkerDone=NULL; m_SharedWorker=NULL; m_SharedWorkerObject=NULL;
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
    m_GpuPostAttempted=FALSE;
#endif
#endif


    for (UINT i=0;i<MAX_VIEWS;i++)
    {
        m_HardwareBlt[i].Initialize(this,i);
    }
}

BASIC_DISPLAY_DRIVER::~BASIC_DISPLAY_DRIVER()
{
    PAGED_CODE();


#if A04_GE8300_WDDM_SHARED_AUTOSTART
    StopSharedDiagnosticWorker();
#endif
    CleanUp();
}
#if A04_GE8300_WDDM_SHARED_AUTOSTART
VOID BASIC_DISPLAY_DRIVER::SharedDiagnosticWorker(PVOID context)
{
    BASIC_DISPLAY_DRIVER *self=(BASIC_DISPLAY_DRIVER *)context;
    KEVENT *done=self->m_SharedWorkerDone;
    A04DisplayTrace(0x83,STATUS_SUCCESS);
    LARGE_INTEGER delay;delay.QuadPart=-10000000LL;
#if A04_GE8300_WDDM_PRESENT_CAPTURE
    /* Snapshot publication and cleanup are protected by m_PresentMutex. GPU
     * work runs without it so normal presents are not blocked for 1000 tiles.
     * StopDevice/D3 join this worker before touching its resources. */
    for(ULONG i=0;i<120;i++) {
        NTSTATUS wait=KeWaitForSingleObject(&self->m_SharedWorkerStop,Executive,KernelMode,FALSE,&delay);
        if(wait!=STATUS_TIMEOUT)break;
        wait=KeWaitForSingleObject(&self->m_PresentMutex,Executive,KernelMode,FALSE,NULL);
        if(!NT_SUCCESS(wait))break;
        if(!self->m_Flags.DriverStarted || !self->m_DisplayResources.Adapter) {
            KeReleaseMutex(&self->m_PresentMutex,FALSE);break;
        }
        if(!self->m_DisplayResources.PresentCapture && !self->m_DisplayResources.PresentCaptureConsumed && i<29u) {
            KeReleaseMutex(&self->m_PresentMutex,FALSE);continue;
        }
        BOOLEAN captured=self->m_DisplayResources.PresentCapture!=NULL;
        self->m_DisplayResources.PresentCaptureConsumed=TRUE;
        KeReleaseMutex(&self->m_PresentMutex,FALSE);
        A04DisplayTrace(0x9c,captured?1:0);
        NTSTATUS status=self->m_DisplayResources.PrepareViaGpu();
        BOOLEAN finished=NT_SUCCESS(status) || self->m_DisplayResources.Shared.mdl ||
            self->m_DisplayResources.GpuHandle || !a04_common_may_attempt(&self->m_DisplayResources.SharedCommonOnce);
        if(i==0 || finished || (i%10)==9)A04DisplayTrace(0x31,status);
        if(finished) {
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
            /* The synthetic fallback can reproduce the known POST display.
             * Never paint an old captured present over newer presents. */
            if(!captured && NT_SUCCESS(status)) {
                wait=KeWaitForSingleObject(&self->m_PresentMutex,Executive,KernelMode,FALSE,NULL);
                if(NT_SUCCESS(wait)) {
                    if(!self->m_GpuPostAttempted) {self->m_GpuPostAttempted=TRUE;
                        NTSTATUS shown=self->ShowVerifiedGpuFrame();A04DisplayTrace(0x81,shown);}
                    KeReleaseMutex(&self->m_PresentMutex,FALSE);
                }
            }
#endif
            break;
        }
    }
    if(NT_SUCCESS(KeWaitForSingleObject(&self->m_PresentMutex,Executive,KernelMode,FALSE,NULL))) {
        if(self->m_DisplayResources.PresentCapture) {
            ExFreePoolWithTag((PVOID)self->m_DisplayResources.PresentCapture,'cG4A');
            self->m_DisplayResources.PresentCapture=NULL;
        }
        self->m_DisplayResources.PresentCaptureConsumed=TRUE;
        KeReleaseMutex(&self->m_PresentMutex,FALSE);
    }
#else
    for(ULONG i=0;i<120;i++) {
        NTSTATUS wait=KeWaitForSingleObject(&self->m_SharedWorkerStop,Executive,KernelMode,FALSE,&delay);
        if(wait==STATUS_SUCCESS)break;
        if(wait!=STATUS_TIMEOUT)break;
        wait=KeWaitForSingleObject(&self->m_PresentMutex,Executive,KernelMode,FALSE,NULL);
        if(!NT_SUCCESS(wait))break;
        NTSTATUS status=STATUS_DEVICE_NOT_READY;
        BOOLEAN done=FALSE;
        if(self->m_Flags.DriverStarted && self->m_DisplayResources.Adapter &&
           !self->m_DisplayResources.Shared.mdl && !self->m_DisplayResources.GpuHandle) {
            status=self->m_DisplayResources.PrepareViaGpu();
            done=NT_SUCCESS(status) || self->m_DisplayResources.Shared.mdl || self->m_DisplayResources.GpuHandle;
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
            if(!a04_common_may_attempt(&self->m_DisplayResources.SharedCommonOnce))done=TRUE;
#endif
        } else if(!self->m_Flags.DriverStarted || !self->m_DisplayResources.Adapter ||
                  self->m_DisplayResources.Shared.mdl || self->m_DisplayResources.GpuHandle) {
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
            if(self->m_DisplayResources.SharedCommonFinished)status=self->m_DisplayResources.SharedCommonResult;
#endif
            done=TRUE;
        }
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
        if(done && NT_SUCCESS(status) && !self->m_GpuPostAttempted) {
            self->m_GpuPostAttempted=TRUE;
            NTSTATUS shown=self->ShowVerifiedGpuFrame();A04DisplayTrace(0x81,shown);
        }
#endif
        KeReleaseMutex(&self->m_PresentMutex,FALSE);
        if(i==0 || done || (i%10)==9) A04DisplayTrace(0x31,status);
        if(done)break;
    }
#endif
    KeSetEvent(done,IO_NO_INCREMENT,FALSE);
    PsTerminateSystemThread(STATUS_SUCCESS);
}
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
/* Caller holds m_PresentMutex. No OVL or display DMA configuration changes.
 * StopDevice signals the event before waiting for this worker, so restoration
 * also runs when removal begins during the timed display. */
NTSTATUS BASIC_DISPLAY_DRIVER::ShowVerifiedGpuFrame()
{
    const uint32_t *pixels=NULL;CURRENT_BDD_MODE *mode=&m_CurrentModes[0];
    const DXGK_DISPLAY_INFORMATION *info=&mode->DispInfo;
    PVOID mapped=NULL,backup=NULL;BOOLEAN temporary=FALSE;NTSTATUS status;
    LARGE_INTEGER timeout;ULONG row,bytes;
    status=m_DisplayResources.GetVerifiedGpuFrame(&pixels);
    if(!NT_SUCCESS(status))return status;
#if A04_GE8300_WDDM_GPU_DIRECT_POST
    /* The GPU already wrote the visible framebuffer; the CPU only restores
     * the desktop saved before the copy. */
    UNREFERENCED_PARAMETER(mode);UNREFERENCED_PARAMETER(info);UNREFERENCED_PARAMETER(mapped);
    UNREFERENCED_PARAMETER(backup);UNREFERENCED_PARAMETER(temporary);UNREFERENCED_PARAMETER(row);
    UNREFERENCED_PARAMETER(bytes);
    if(!m_DisplayResources.DirectPostActive || !m_DisplayResources.PostBackup)return STATUS_DEVICE_NOT_READY;
    A04DisplayTrace(0x8e,STATUS_SUCCESS);
    timeout.QuadPart=-50000000LL;
    (void)KeWaitForSingleObject(&m_SharedWorkerStop,Executive,KernelMode,FALSE,&timeout);
    RtlCopyMemory((void *)pixels,m_DisplayResources.PostBackup,4096000u);
    KeMemoryBarrier();A04DisplayTrace(0x82,STATUS_SUCCESS);
#if A04_GE8300_WDDM_LIVE_FULL_PRESENT
    if(!KeReadStateEvent(&m_SharedWorkerStop)) {
        m_DisplayResources.LiveReady=TRUE;A04DisplayTrace(0xb4,STATUS_SUCCESS);
    }
#endif
    return STATUS_SUCCESS;
#endif
    if(KeReadStateEvent(&m_SharedWorkerStop) || !m_Flags.DriverStarted ||
       m_AdapterPowerState!=PowerDeviceD0 || m_MonitorPowerState!=PowerDeviceD0 ||
       info->Width!=800u || info->Height!=1280u || info->Pitch<3200u ||
       info->Pitch>MAXULONG/1280u ||
       (info->ColorFormat!=D3DDDIFMT_X8R8G8B8 && info->ColorFormat!=D3DDDIFMT_A8R8G8B8))
        return STATUS_DEVICE_NOT_READY;
    bytes=info->Pitch*1280u;
    backup=ExAllocatePoolWithTag(NonPagedPoolNx,4096000u,'pG4A');
    if(!backup)return STATUS_INSUFFICIENT_RESOURCES;
    mapped=mode->FrameBuffer.Ptr;
    if(!mapped) {
        status=MapFrameBuffer(info->PhysicAddress,bytes,&mapped);
        if(!NT_SUCCESS(status))goto done;
        temporary=TRUE;
    }
    if(KeReadStateEvent(&m_SharedWorkerStop)){status=STATUS_CANCELLED;goto done;}
    for(row=0;row<1280u;row++)
        RtlCopyMemory((UCHAR *)backup+row*3200u,(UCHAR *)mapped+row*info->Pitch,3200u);
    for(row=0;row<1280u;row++)
        RtlCopyMemory((UCHAR *)mapped+row*info->Pitch,(const UCHAR *)pixels+row*3200u,3200u);
    KeMemoryBarrier();A04DisplayTrace(0x80,STATUS_SUCCESS);
    timeout.QuadPart=-50000000LL;
    (void)KeWaitForSingleObject(&m_SharedWorkerStop,Executive,KernelMode,FALSE,&timeout);
    for(row=0;row<1280u;row++)
        RtlCopyMemory((UCHAR *)mapped+row*info->Pitch,(UCHAR *)backup+row*3200u,3200u);
    KeMemoryBarrier();A04DisplayTrace(0x82,STATUS_SUCCESS);status=STATUS_SUCCESS;
done:
    if(temporary)UnmapFrameBuffer(mapped,bytes);
    ExFreePoolWithTag(backup,'pG4A');return status;
}
#endif
VOID BASIC_DISPLAY_DRIVER::StopSharedDiagnosticWorker()
{
    NTSTATUS status=KeWaitForSingleObject(&m_SharedWorkerStopMutex,Executive,KernelMode,FALSE,NULL);
    if(!NT_SUCCESS(status))return;
    A04PresentGuard stopGuard(&m_SharedWorkerStopMutex);
    if(!m_SharedWorker)return;
    KeSetEvent(&m_SharedWorkerStop,IO_NO_INCREMENT,FALSE);
    if(m_SharedWorkerObject) {
        (void)KeWaitForSingleObject(m_SharedWorkerObject,Executive,KernelMode,FALSE,NULL);
        ObDereferenceObject(m_SharedWorkerObject);m_SharedWorkerObject=NULL;
        ExFreePoolWithTag(m_SharedWorkerDone,'wG4A');
    } else {
        /* Rare handle-reference failure: do not free the separately allocated
         * event while KeSetEvent may still be returning on the worker CPU. */
        (void)KeWaitForSingleObject(m_SharedWorkerDone,Executive,KernelMode,FALSE,NULL);
    }
    m_SharedWorkerDone=NULL;ZwClose(m_SharedWorker);m_SharedWorker=NULL;
}
#endif

NTSTATUS BASIC_DISPLAY_DRIVER::StartDevice(_In_  DXGK_START_INFO*   pDxgkStartInfo,
                                           _In_  DXGKRNL_INTERFACE* pDxgkInterface,
                                           _Out_ ULONG*             pNumberOfViews,
                                           _Out_ ULONG*             pNumberOfChildren)
{
    PAGED_CODE();struct geo_gap359_record gap;Geo359Begin(&gap,3);

    BDD_ASSERT(pDxgkStartInfo != NULL);
    BDD_ASSERT(pDxgkInterface != NULL);
    BDD_ASSERT(pNumberOfViews != NULL);
    BDD_ASSERT(pNumberOfChildren != NULL);

    RtlCopyMemory(&m_StartInfo, pDxgkStartInfo, sizeof(m_StartInfo));
    RtlCopyMemory(&m_DxgkInterface, pDxgkInterface, sizeof(m_DxgkInterface));
    RtlZeroMemory(m_CurrentModes, sizeof(m_CurrentModes));
    m_CurrentModes[0].DispInfo.TargetId = D3DDDI_ID_UNINITIALIZED;

    // Get device information from OS.
    Geo359Enter(&gap,0);NTSTATUS Status = m_DxgkInterface.DxgkCbGetDeviceInformation(m_DxgkInterface.DeviceHandle, &m_DeviceInfo);Geo359Result(&gap,0,Status);
    if (!NT_SUCCESS(Status))
    {
        BDD_LOG_ASSERTION1("DxgkCbGetDeviceInformation failed with status 0x%I64x",
                           Status);
        return Geo359End(&gap,Status);
    }

    // Ignore return value, since it's not the end of the world if we failed to write these values to the registry
    RegisterHWInfo();

    // TODO: Uncomment the line below after updating the TODOs in the function CheckHardware
//    Status = CheckHardware();
    if (!NT_SUCCESS(Status))
    {
        return Geo359End(&gap,Status);
    }

    // This sample driver only uses the frame buffer of the POST device. DxgkCbAcquirePostDisplayOwnership
    // gives you the frame buffer address and ensures that no one else is drawing to it. Be sure to give it back!
    Geo359Enter(&gap,1);Status = m_DxgkInterface.DxgkCbAcquirePostDisplayOwnership(m_DxgkInterface.DeviceHandle, &(m_CurrentModes[0].DispInfo));Geo359Result(&gap,1,Status);
    gap.value[0]=m_CurrentModes[0].DispInfo.Width;gap.value[1]=m_CurrentModes[0].DispInfo.Height;gap.value[2]=m_CurrentModes[0].DispInfo.Pitch;gap.value[3]=(ULONG)m_CurrentModes[0].DispInfo.ColorFormat;gap.value[4]=m_CurrentModes[0].DispInfo.PhysicAddress.LowPart;gap.value[5]=(ULONG)m_CurrentModes[0].DispInfo.PhysicAddress.HighPart;gap.value[6]=m_CurrentModes[0].DispInfo.TargetId;gap.value[7]=m_CurrentModes[0].DispInfo.AcpiId;
    if (!NT_SUCCESS(Status) || m_CurrentModes[0].DispInfo.Width == 0)
    {
        // The most likely cause of failure is that the driver is simply not running on a POST device, or we are running
        // after a pre-WDDM 1.2 driver. Since we can't draw anything, we should fail to start.
        return Geo359End(&gap,STATUS_UNSUCCESSFUL);
    }
    {
        const DXGK_DISPLAY_INFORMATION* info=&m_CurrentModes[0].DispInfo;
        uint32_t w,h,offset;
        /* The map, blackout and unmap paths use Pitch * Height. Reject
         * unsupported or overflowing POST surfaces before any of them run. */
        Geo359Enter(&gap,2);if (BPPFromPixelFormat(info->ColorFormat)!=32 ||
            a04_present_layout(info->Width,info->Height,info->Width,info->Height,
                               info->Pitch,0,0,&w,&h,&offset) ||
            (uint64_t)info->PhysicAddress.QuadPart>UINT64_MAX-(uint64_t)info->Pitch*info->Height)
            return Geo359End(&gap,STATUS_DEVICE_CONFIGURATION_ERROR);
        Geo359Result(&gap,2,STATUS_SUCCESS);
    }
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
#if A04_GE8300_WDDM_GPU_DIRECT_POST
    m_DisplayResources.PostPhys=m_CurrentModes[0].DispInfo.PhysicAddress;
    m_DisplayResources.PostPitch=m_CurrentModes[0].DispInfo.Pitch;
#endif
    /* Absence of explicit OVL resources keeps the CPU display path working. */
    Status=m_DisplayResources.Start(m_pPhysicalDevice,&m_DeviceInfo);
    A04DisplayTrace(0x3f,Status);
    if (!NT_SUCCESS(Status)) {
        DbgPrint("A04 display: direct resources unavailable: 0x%08lx\n",Status);
    }
#endif
#if A04_MT8167_DISPLAY_BRIGHTNESS
    m_CurrentModes[0].Flags.IsInternal=TRUE; // Fixed built-in A04BR3 portrait panel.
    NTSTATUS brightnessStatus=m_Brightness.Start(A04_GE8300_UNIFIED_DISPLAY356?m_UnifiedBrightnessResources:m_DeviceInfo.TranslatedResourceList);
    m_Brightness.SetPower(m_AdapterPowerState==PowerDeviceD0 && m_MonitorPowerState==PowerDeviceD0);
    A04DisplayTrace(0xb2,brightnessStatus); // Optional PWM resource never blocks the POST display.
#endif
    m_Flags.DriverStarted = TRUE;
#if A04_GE8300_WDDM_SHARED_AUTOSTART
    if(m_DisplayResources.Adapter && !m_SharedWorker) {
        KeClearEvent(&m_SharedWorkerStop);
        m_SharedWorkerDone=(KEVENT *)ExAllocatePoolWithTag(NonPagedPoolNx,sizeof(KEVENT),'wG4A');
        NTSTATUS workerStatus=STATUS_INSUFFICIENT_RESOURCES;
        if(m_SharedWorkerDone) {
            KeInitializeEvent(m_SharedWorkerDone,NotificationEvent,FALSE);
            workerStatus=PsCreateSystemThread(&m_SharedWorker,THREAD_ALL_ACCESS,NULL,NULL,NULL,
                SharedDiagnosticWorker,this);
            if(NT_SUCCESS(workerStatus)) {
                workerStatus=ObReferenceObjectByHandle(m_SharedWorker,THREAD_ALL_ACCESS,NULL,KernelMode,
                    &m_SharedWorkerObject,NULL);
                if(!NT_SUCCESS(workerStatus))StopSharedDiagnosticWorker();
            } else { ExFreePoolWithTag(m_SharedWorkerDone,'wG4A');m_SharedWorkerDone=NULL; }
        }
        A04DisplayTrace(0x30,workerStatus);
    } else A04DisplayTrace(0x30,STATUS_DEVICE_NOT_READY);
#endif
    *pNumberOfViews = MAX_VIEWS;
    *pNumberOfChildren = MAX_CHILDREN;


   return Geo359End(&gap,STATUS_SUCCESS);
}

NTSTATUS BASIC_DISPLAY_DRIVER::StopDevice(VOID)
{
    PAGED_CODE();
#if A04_GE8300_WDDM_SHARED_AUTOSTART
    StopSharedDiagnosticWorker();
#endif

    NTSTATUS status=KeWaitForSingleObject(&m_PresentMutex,Executive,KernelMode,FALSE,NULL);
    if (!NT_SUCCESS(status)) return status;
    A04PresentGuard guard(&m_PresentMutex);
    m_Flags.DriverStarted = FALSE;
    CleanUp();
    return STATUS_SUCCESS;
}

VOID BASIC_DISPLAY_DRIVER::CleanUp()
{
    PAGED_CODE();
#if A04_MT8167_DISPLAY_BRIGHTNESS
    m_Brightness.Stop();
#endif
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
    m_DisplayResources.Stop();
#endif

    for (UINT Source = 0; Source < MAX_VIEWS; ++Source)
    {
        if (m_CurrentModes[Source].FrameBuffer.Ptr)
        {
            UnmapFrameBuffer(m_CurrentModes[Source].FrameBuffer.Ptr, m_CurrentModes[Source].DispInfo.Height * m_CurrentModes[Source].DispInfo.Pitch);
            m_CurrentModes[Source].FrameBuffer.Ptr = NULL;
            m_CurrentModes[Source].Flags.FrameBufferIsActive = FALSE;
        }
    }

}


NTSTATUS BASIC_DISPLAY_DRIVER::DispatchIoRequest(_In_  ULONG                 VidPnSourceId,
                                                 _In_  VIDEO_REQUEST_PACKET* pVideoRequestPacket)
{
    PAGED_CODE();

    BDD_ASSERT(pVideoRequestPacket != NULL);
    BDD_ASSERT(VidPnSourceId < MAX_VIEWS);

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS BASIC_DISPLAY_DRIVER::SetPowerState(_In_  ULONG              HardwareUid,
                                             _In_  DEVICE_POWER_STATE DevicePowerState,
                                             _In_  POWER_ACTION       ActionType)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ActionType);

    BDD_ASSERT((HardwareUid < MAX_CHILDREN) || (HardwareUid == DISPLAY_ADAPTER_HW_ID));

    if (HardwareUid == DISPLAY_ADAPTER_HW_ID)
    {
#if A04_GE8300_WDDM_GPU_POST_DIAGNOSTIC
        A04DisplayTrace(0x84,(NTSTATUS)DevicePowerState);
        if(DevicePowerState!=PowerDeviceD0)StopSharedDiagnosticWorker();
#endif
        NTSTATUS lockStatus=KeWaitForSingleObject(&m_PresentMutex,Executive,KernelMode,FALSE,NULL);
        if(!NT_SUCCESS(lockStatus))return lockStatus;
        A04PresentGuard powerGuard(&m_PresentMutex);
#if A04_GE8300_WDDM_LIVE_FULL_PRESENT
        m_DisplayResources.LiveShadowValid=FALSE;
#endif
        if (DevicePowerState == PowerDeviceD0)
        {

            // When returning from D3 the device visibility defined to be off for all targets
            if (m_AdapterPowerState == PowerDeviceD3)
            {
                /* No provider IOCTL or POST write in a power callback:
                 * the independently powered provider may not be in D0 yet. */
                for(UINT i=0;i<MAX_VIEWS;i++) {
                    m_CurrentModes[i].Flags.SourceNotVisible=TRUE;
                    m_CurrentModes[i].Flags.FullscreenPresent=TRUE;
                }
            }
        }

        // Store new adapter power state
        m_AdapterPowerState = DevicePowerState;
#if A04_MT8167_DISPLAY_BRIGHTNESS
        m_Brightness.SetPower(m_AdapterPowerState==PowerDeviceD0 && m_MonitorPowerState==PowerDeviceD0);
#endif

        // There is nothing to do to specifically power up/down the display adapter
        return STATUS_SUCCESS;
    }
    else
    {
        NTSTATUS lockStatus=KeWaitForSingleObject(&m_PresentMutex,Executive,KernelMode,FALSE,NULL);
        if(!NT_SUCCESS(lockStatus))return lockStatus;
        A04PresentGuard monitorGuard(&m_PresentMutex);
        m_MonitorPowerState=DevicePowerState;
#if A04_MT8167_DISPLAY_BRIGHTNESS
        m_Brightness.SetPower(m_AdapterPowerState==PowerDeviceD0 && m_MonitorPowerState==PowerDeviceD0);
#endif
#if A04_GE8300_WDDM_LIVE_FULL_PRESENT
        m_DisplayResources.LiveShadowValid=FALSE;
#endif
        return STATUS_SUCCESS;
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildRelations(_Out_writes_bytes_(ChildRelationsSize) DXGK_CHILD_DESCRIPTOR* pChildRelations,
                                                   _In_                             ULONG                  ChildRelationsSize)
{
    PAGED_CODE();

    BDD_ASSERT(pChildRelations != NULL);

    // The last DXGK_CHILD_DESCRIPTOR in the array of pChildRelations must remain zeroed out, so we subtract this from the count
    ULONG ChildRelationsCount = (ChildRelationsSize / sizeof(DXGK_CHILD_DESCRIPTOR)) - 1;
    BDD_ASSERT(ChildRelationsCount <= MAX_CHILDREN);

    for (UINT ChildIndex = 0; ChildIndex < ChildRelationsCount; ++ChildIndex)
    {
        pChildRelations[ChildIndex].ChildDeviceType = TypeVideoOutput;
        pChildRelations[ChildIndex].ChildCapabilities.HpdAwareness = HpdAwarenessInterruptible;
        pChildRelations[ChildIndex].ChildCapabilities.Type.VideoOutput.InterfaceTechnology = m_CurrentModes[0].Flags.IsInternal ? D3DKMDT_VOT_INTERNAL : D3DKMDT_VOT_OTHER;
        pChildRelations[ChildIndex].ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness = D3DKMDT_MOA_NONE;
        pChildRelations[ChildIndex].ChildCapabilities.Type.VideoOutput.SupportsSdtvModes = FALSE;
        // TODO: Replace 0 with the actual ACPI ID of the child device, if available
        pChildRelations[ChildIndex].AcpiUid = 0;
        pChildRelations[ChildIndex].ChildUid = ChildIndex;
    }

    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryChildStatus(_Inout_ DXGK_CHILD_STATUS* pChildStatus,
                                                _In_    BOOLEAN            NonDestructiveOnly)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(NonDestructiveOnly);
    BDD_ASSERT(pChildStatus != NULL);
    BDD_ASSERT(pChildStatus->ChildUid < MAX_CHILDREN);

    switch (pChildStatus->Type)
    {
        case StatusConnection:
        {
            // HpdAwarenessInterruptible was reported since HpdAwarenessNone is deprecated.
            // However, BDD has no knowledge of HotPlug events, so just always return connected.
            pChildStatus->HotPlug.Connected = IsDriverActive();
            return STATUS_SUCCESS;
        }

        case StatusRotation:
        {
            // D3DKMDT_MOA_NONE was reported, so this should never be called
            BDD_LOG_ERROR0("Child status being queried for StatusRotation even though D3DKMDT_MOA_NONE was reported");
            return STATUS_INVALID_PARAMETER;
        }

        default:
        {
            BDD_LOG_WARNING1("Unknown pChildStatus->Type (0x%I64x) requested.", pChildStatus->Type);
            return STATUS_NOT_SUPPORTED;
        }
    }
}

// EDID retrieval
NTSTATUS BASIC_DISPLAY_DRIVER::QueryDeviceDescriptor(_In_    ULONG                   ChildUid,
                                                     _Inout_ DXGK_DEVICE_DESCRIPTOR* pDeviceDescriptor)
{
    PAGED_CODE();

    BDD_ASSERT(pDeviceDescriptor != NULL);
    BDD_ASSERT(ChildUid < MAX_CHILDREN);

    // If we haven't successfully retrieved an EDID yet (invalid ones are ok, so long as it was retrieved)
    if (!m_Flags.EDID_Attempted)
    {
        GetEdid(ChildUid);
    }

    if (!m_Flags.EDID_Retrieved || !m_Flags.EDID_ValidHeader || !m_Flags.EDID_ValidChecksum)
    {
        // Report no EDID if a valid one wasn't retrieved
        return STATUS_GRAPHICS_CHILD_DESCRIPTOR_NOT_SUPPORTED;
    }
    else if (pDeviceDescriptor->DescriptorOffset == 0)
    {
        // Only the base block is supported
        RtlCopyMemory(pDeviceDescriptor->DescriptorBuffer,
                      m_EDIDs[ChildUid],
                      min(pDeviceDescriptor->DescriptorLength, EDID_V1_BLOCK_SIZE));

        return STATUS_SUCCESS;
    }
    else
    {
        return STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA;
    }
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryAdapterInfo(_In_ CONST DXGKARG_QUERYADAPTERINFO* pQueryAdapterInfo)
{
    PAGED_CODE();

    BDD_ASSERT(pQueryAdapterInfo != NULL);

    switch (pQueryAdapterInfo->Type)
    {
        case DXGKQAITYPE_DRIVERCAPS:
        {
            if (pQueryAdapterInfo->OutputDataSize < sizeof(DXGK_DRIVERCAPS))
            {
                BDD_LOG_ERROR2("pQueryAdapterInfo->OutputDataSize (0x%I64x) is smaller than sizeof(DXGK_DRIVERCAPS) (0x%I64x)", pQueryAdapterInfo->OutputDataSize, sizeof(DXGK_DRIVERCAPS));
                return STATUS_BUFFER_TOO_SMALL;
            }

            DXGK_DRIVERCAPS* pDriverCaps = (DXGK_DRIVERCAPS*)pQueryAdapterInfo->pOutputData;

            // Nearly all fields must be initialized to zero, so zero out to start and then change those that are non-zero.
            // Fields are zero since BDD is Display-Only and therefore does not support any of the render related fields.
            // It also doesn't support hardware interrupts, gamma ramps, etc.
            RtlZeroMemory(pDriverCaps, sizeof(DXGK_DRIVERCAPS));

            pDriverCaps->WDDMVersion = DXGKDDI_WDDMv1_2;
            pDriverCaps->HighestAcceptableAddress.QuadPart = -1;

            pDriverCaps->SupportNonVGA = TRUE;
            pDriverCaps->SupportSmoothRotation = TRUE;

            return STATUS_SUCCESS;
        }

#if NTDDI_VERSION >= 0x0A000000
        case DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION:
        {
            DXGK_DISPLAY_DRIVERCAPS_EXTENSION* pDriverDisplayCaps;

            if (pQueryAdapterInfo->OutputDataSize < sizeof(*pDriverDisplayCaps))
            {
                BDD_LOG_ERROR2("pQueryAdapterInfo->OutputDataSize (0x%I64x) is smaller than sizeof(DXGK_DISPLAY_DRIVERCAPS_EXTENSION) (0x%I64x)",
                               pQueryAdapterInfo->OutputDataSize,
                               sizeof(DXGK_DISPLAY_DRIVERCAPS_EXTENSION));

                return STATUS_INVALID_PARAMETER;
            }

            pDriverDisplayCaps = (DXGK_DISPLAY_DRIVERCAPS_EXTENSION*)pQueryAdapterInfo->pOutputData;

            // Reset all caps values
            RtlZeroMemory(pDriverDisplayCaps, pQueryAdapterInfo->OutputDataSize);

            // We claim to support virtual display mode.
            pDriverDisplayCaps->VirtualModeSupport = 1;

            return STATUS_SUCCESS;
        }

#endif

        default:
        {
            // BDD does not need to support any other adapter information types
            BDD_LOG_WARNING1("Unknown QueryAdapterInfo Type (0x%I64x) requested", pQueryAdapterInfo->Type);
            return STATUS_NOT_SUPPORTED;
        }
    }
}


NTSTATUS BASIC_DISPLAY_DRIVER::CheckHardware()
{
    PAGED_CODE();

    NTSTATUS Status;
    ULONG VendorID;
    ULONG DeviceID;

// TODO: If developing a driver for PCI based hardware, then use the second method to retrieve Vendor/Device IDs.
// If developing for non-PCI based hardware (i.e. ACPI based hardware), use the first method to retrieve the IDs.
#if 1 // ACPI-based device

    // Get the Vendor & Device IDs on non-PCI system
    ACPI_EVAL_INPUT_BUFFER_COMPLEX AcpiInputBuffer = {0};
    AcpiInputBuffer.Signature = ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
    AcpiInputBuffer.MethodNameAsUlong = ACPI_METHOD_HARDWARE_ID;
    AcpiInputBuffer.Size = 0;
    AcpiInputBuffer.ArgumentCount = 0;

    BYTE OutputBuffer[sizeof(ACPI_EVAL_OUTPUT_BUFFER) + 0x10];
    RtlZeroMemory(OutputBuffer, sizeof(OutputBuffer));
    ACPI_EVAL_OUTPUT_BUFFER* pAcpiOutputBuffer = reinterpret_cast<ACPI_EVAL_OUTPUT_BUFFER*>(&OutputBuffer);

    Status = m_DxgkInterface.DxgkCbEvalAcpiMethod(m_DxgkInterface.DeviceHandle,
                                                  DISPLAY_ADAPTER_HW_ID,
                                                  &AcpiInputBuffer,
                                                  sizeof(AcpiInputBuffer),
                                                  pAcpiOutputBuffer,
                                                  sizeof(OutputBuffer));
    if (!NT_SUCCESS(Status))
    {
        BDD_LOG_ERROR1("DxgkCbReadDeviceSpace failed to get hardware IDs with status 0x%I64x", Status);
        return Status;
    }

    VendorID = ((ULONG*)(pAcpiOutputBuffer->Argument[0].Data))[0];
    DeviceID = ((ULONG*)(pAcpiOutputBuffer->Argument[0].Data))[1];

#else // PCI-based device

    // Get the Vendor & Device IDs on PCI system
    PCI_COMMON_HEADER Header = {0};
    ULONG BytesRead;

    Status = m_DxgkInterface.DxgkCbReadDeviceSpace(m_DxgkInterface.DeviceHandle,
                                                   DXGK_WHICHSPACE_CONFIG,
                                                   &Header,
                                                   0,
                                                   sizeof(Header),
                                                   &BytesRead);

    if (!NT_SUCCESS(Status))
    {
        BDD_LOG_ERROR1("DxgkCbReadDeviceSpace failed with status 0x%I64x", Status);
        return Status;
    }

    VendorID = Header.VendorID;
    DeviceID = Header.DeviceID;

#endif

    // TODO: Replace 0x1414 with your Vendor ID
    if (VendorID == 0x1414)
    {
        switch (DeviceID)
        {
            // TODO: Replace the case statements below with the Device IDs supported by this driver
            case 0x0000:
            case 0xFFFF: return STATUS_SUCCESS;
        }
    }

    return STATUS_GRAPHICS_DRIVER_MISMATCH;
}

// Even though Sample Basic Display Driver does not support hardware cursors, and reports such
// in QueryAdapterInfo. This function can still be called to set the pointer to not visible
NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerPosition(_In_ CONST DXGKARG_SETPOINTERPOSITION* pSetPointerPosition)
{
    PAGED_CODE();

    BDD_ASSERT(pSetPointerPosition != NULL);
    BDD_ASSERT(pSetPointerPosition->VidPnSourceId < MAX_VIEWS);

    if (!(pSetPointerPosition->Flags.Visible))
    {
        return STATUS_SUCCESS;
    }
    else
    {
        BDD_LOG_ASSERTION0("SetPointerPosition should never be called to set the pointer to visible since BDD doesn't support hardware cursors.");
        return STATUS_UNSUCCESSFUL;
    }
}

// Basic Sample Display Driver does not support hardware cursors, and reports such
// in QueryAdapterInfo. Therefore this function should never be called.
NTSTATUS BASIC_DISPLAY_DRIVER::SetPointerShape(_In_ CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape)
{
    PAGED_CODE();

    BDD_ASSERT(pSetPointerShape != NULL);
    BDD_LOG_ASSERTION0("SetPointerShape should never be called since BDD doesn't support hardware cursors.");

    return STATUS_NOT_IMPLEMENTED;
}
#if A04_GE8300_WDDM_LIVE_FULL_PRESENT
/* Called only by the synchronous DOD path with m_PresentMutex held. */
NTSTATUS BASIC_DISPLAY_DRIVER::TryGpuLogicalPresent(const BLT_INFO *source,const BLT_INFO *destination,
    ULONG moves,const D3DKMT_MOVE_RECT *move,ULONG count,const RECT *rects)
{
    const CURRENT_BDD_MODE *mode=&m_CurrentModes[0];uint32_t w,h,offset;unsigned turns;
    A04DisplayResources *resources=&m_DisplayResources;
    if(resources->LiveCpuFallback)return STATUS_NOT_SUPPORTED;
    if(resources->LiveReady && KeReadStateEvent(&m_SharedWorkerStop)) {
        NTSTATUS retired=resources->RetireToCpu();
        return NT_SUCCESS(retired)?STATUS_NOT_SUPPORTED:retired;
    }
    if(!source || !destination || (moves && !move) || (count && !rects) || !m_Flags.DriverStarted ||
       m_AdapterPowerState!=PowerDeviceD0 || m_MonitorPowerState!=PowerDeviceD0 ||
       KeReadStateEvent(&m_SharedWorkerStop) || mode->DispInfo.Width!=800u || mode->DispInfo.Height!=1280u ||
       mode->DispInfo.Pitch!=3200u || !mode->FrameBuffer.Ptr ||
       mode->DispInfo.PhysicAddress.QuadPart!=resources->PostPhys.QuadPart || !resources->LiveReady) {
        resources->LiveShadowValid=FALSE;return STATUS_NOT_SUPPORTED;
    }
    switch(destination->Rotation) {
    case D3DKMDT_VPPR_IDENTITY:turns=0;break;
    case D3DKMDT_VPPR_ROTATE90:turns=1;break;
    case D3DKMDT_VPPR_ROTATE180:turns=2;break;
    case D3DKMDT_VPPR_ROTATE270:turns=3;break;
    default:resources->LiveShadowValid=FALSE;return STATUS_NOT_SUPPORTED;
    }
    if(a04_present_layout(mode->SrcModeWidth,mode->SrcModeHeight,800,1280,3200,turns,
           mode->Scaling==D3DKMDT_VPPS_CENTERED,&w,&h,&offset) ||
       source->Width!=mode->SrcModeWidth || source->Height!=mode->SrcModeHeight ||
       destination->Width!=w || destination->Height!=h || destination->Pitch!=3200 ||
       destination->BitsPerPel!=32 || source->BitsPerPel!=32 ||
       destination->pBits!=(UCHAR *)mode->FrameBuffer.Ptr+offset) {
        resources->LiveShadowValid=FALSE;return STATUS_NOT_SUPPORTED;
    }
    if(!moves && !count)return STATUS_SUCCESS;
    void *staging=ExAllocatePoolWithTag(NonPagedPoolNx,4096000u,'rG4A');
    if(!staging)return STATUS_INSUFFICIENT_RESOURCES;
    BOOLEAN cached=resources->LiveShadowValid && resources->LiveShadow &&
        resources->LiveWidth==mode->SrcModeWidth && resources->LiveHeight==mode->SrcModeHeight &&
        resources->LiveRotation==turns && resources->LiveOffset==offset;
    /* Seed once from the owned native POST image. Subsequent presents update
     * a cached native shadow with exactly the legacy DOD move/dirty blits.
     * Unsupported CPU fallback, blackout and viewport changes invalidate it. */
    RtlCopyMemory(staging,cached?resources->LiveShadow:mode->FrameBuffer.Ptr,4096000u);
    BLT_INFO rotated=*destination;rotated.pBits=(UCHAR *)staging+offset;
    for(ULONG i=0;i<moves;i++)BltBits(&rotated,source,1,&move[i].DestRect);
    for(ULONG i=0;i<count;i++)BltBits(&rotated,source,1,&rects[i]);
    NTSTATUS status=resources->SubmitLiveFrame(staging,3200u);
    if(status==STATUS_NOT_SUPPORTED && resources->LiveCpuFallback) {
        /* A failed full GPU copy may have written part of POST. Restore the
         * entire intended frame after retirement, not only its dirty rects. */
        WRITE_REGISTER_BUFFER_ULONG((volatile ULONG *)mode->FrameBuffer.Ptr,(ULONG *)staging,1024000u);
        status=STATUS_SUCCESS;
#if A04_GE8300_IDLE_TRANSITION_DIAGNOSTIC
        if(!resources->IdleRestoreChecked) {
            ULONG matched=0;resources->IdleRestoreChecked=TRUE;KeMemoryBarrier();
            for(ULONG i=0;i<1024000u;i++)
                if(READ_REGISTER_ULONG((volatile ULONG *)mode->FrameBuffer.Ptr+i)==((ULONG *)staging)[i])matched++;
            A04DisplayTrace(0xc5,(NTSTATUS)matched);
            status=matched==1024000u?STATUS_SUCCESS:STATUS_DATA_ERROR;
            A04DisplayTrace(0xc6,status);
            if(!NT_SUCCESS(status))resources->LiveFaulted=TRUE;
        }
#endif
    }
    if(NT_SUCCESS(status)) {
        void *previous=resources->LiveShadow;resources->LiveShadow=staging;staging=NULL;
        resources->LiveShadowValid=TRUE;resources->LiveWidth=mode->SrcModeWidth;
        resources->LiveHeight=mode->SrcModeHeight;resources->LiveRotation=turns;resources->LiveOffset=offset;
        if(previous)ExFreePoolWithTag(previous,'rG4A');
    }
    if(staging)ExFreePoolWithTag(staging,'rG4A');return status;
}
#endif
NTSTATUS BASIC_DISPLAY_DRIVER::PresentDisplayOnly(_In_ CONST DXGKARG_PRESENT_DISPLAYONLY* pPresentDisplayOnly)
{
    PAGED_CODE();

    NTSTATUS waitStatus=KeWaitForSingleObject(&m_PresentMutex,Executive,KernelMode,FALSE,NULL);
    if (!NT_SUCCESS(waitStatus)) return waitStatus;
    A04PresentGuard guard(&m_PresentMutex);

    if (!pPresentDisplayOnly || pPresentDisplayOnly->VidPnSourceId >= MAX_VIEWS)
        return STATUS_INVALID_PARAMETER;
    if (!m_Flags.DriverStarted) return STATUS_DEVICE_NOT_READY;
#if A04_GE8300_WDDM_LIVE_FULL_PRESENT
    /* A previous unretired failure discarded its intended frame. Never
     * treat a later power acknowledgement as recovery of those pixels. */
    if(m_DisplayResources.LiveFaulted)return STATUS_IO_DEVICE_ERROR;
#endif

    if (pPresentDisplayOnly->BytesPerPixel < MIN_BYTES_PER_PIXEL_REPORTED)
    {
        // Only >=32bpp modes are reported, therefore this Present should never pass anything less than 4 bytes per pixel
        BDD_LOG_ERROR1("pPresentDisplayOnly->BytesPerPixel is 0x%I64x, which is lower than the allowed.", pPresentDisplayOnly->BytesPerPixel);
        return STATUS_INVALID_PARAMETER;
    }

    // If it is in monitor off state or source is not supposed to be visible, don't present anything to the screen
    if ((m_AdapterPowerState > PowerDeviceD0) || (m_MonitorPowerState > PowerDeviceD0) ||
        (m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Flags.SourceNotVisible))
    {
        return STATUS_SUCCESS;
    }

    // Present is only valid if the target is actively connected to this source
    if (m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Flags.FrameBufferIsActive)
    {

        // If actual pixels are coming through, will need to completely zero out physical address next time in BlackOutScreen
        m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].ZeroedOutStart.QuadPart = 0;
        m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].ZeroedOutEnd.QuadPart = 0;


        D3DKMDT_VIDPN_PRESENT_PATH_ROTATION RotationNeededByFb = pPresentDisplayOnly->Flags.Rotate ?
                                                                 m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Rotation :
                                                                 D3DKMDT_VPPR_IDENTITY;
            BYTE* pDst = (BYTE*)m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].FrameBuffer.Ptr;
            UINT DstBitPerPixel = BPPFromPixelFormat(m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].DispInfo.ColorFormat);
            const CURRENT_BDD_MODE* mode=&m_CurrentModes[pPresentDisplayOnly->VidPnSourceId];
            uint32_t physicalWidth,physicalHeight,offset,sourceWidth,sourceHeight; unsigned turns,pathTurns;
            switch (RotationNeededByFb) {
            case D3DKMDT_VPPR_IDENTITY: turns=0; break;
            case D3DKMDT_VPPR_ROTATE90: turns=1; break;
            case D3DKMDT_VPPR_ROTATE180: turns=2; break;
            case D3DKMDT_VPPR_ROTATE270: turns=3; break;
            default: return STATUS_INVALID_PARAMETER;
            }
            if (!pDst || DstBitPerPixel!=32 || a04_present_turns((unsigned)mode->Rotation,&pathTurns) ||
                a04_present_source_shape(mode->SrcModeWidth,mode->SrcModeHeight,pathTurns,turns,&sourceWidth,&sourceHeight) ||
                a04_present_layout(sourceWidth,sourceHeight,mode->DispInfo.Width,
                    mode->DispInfo.Height,mode->DispInfo.Pitch,turns,
                    mode->Scaling==D3DKMDT_VPPS_CENTERED,&physicalWidth,&physicalHeight,&offset) ||
                (uintptr_t)pDst>UINTPTR_MAX-(uint64_t)mode->DispInfo.Pitch*mode->DispInfo.Height)
                return STATUS_INVALID_PARAMETER;
            pDst+=offset;
            NTSTATUS presentStatus=m_HardwareBlt[pPresentDisplayOnly->VidPnSourceId].ExecutePresentDisplayOnly(pDst,
                                                                    DstBitPerPixel,
                                                                    (BYTE*)pPresentDisplayOnly->pSource,
                                                                    pPresentDisplayOnly->BytesPerPixel,
                                                                    pPresentDisplayOnly->Pitch,
                                                                    pPresentDisplayOnly->NumMoves,
                                                                    pPresentDisplayOnly->pMoves,
                                                                    pPresentDisplayOnly->NumDirtyRects,
                                                                    pPresentDisplayOnly->pDirtyRect,
                                                                    RotationNeededByFb);
            if(NT_SUCCESS(presentStatus))m_CurrentModes[pPresentDisplayOnly->VidPnSourceId].Flags.FullscreenPresent=FALSE;
#if A04_GE8300_WDDM_PRESENT_CAPTURE
            /* A snapshot of the completed POST image incorporates move and
             * dirty rectangles. This is staging for GPU copy, not scanout. */
            if(NT_SUCCESS(presentStatus) && pPresentDisplayOnly->VidPnSourceId==0 &&
               m_SharedWorker && !m_DisplayResources.PresentCapture &&
               !m_DisplayResources.PresentCaptureConsumed && !KeReadStateEvent(&m_SharedWorkerStop) &&
               mode->DispInfo.Width==800u && mode->DispInfo.Height==1280u &&
               mode->DispInfo.Pitch>=3200u && mode->DispInfo.Pitch<=MAXULONG/1280u &&
               (mode->DispInfo.ColorFormat==D3DDDIFMT_X8R8G8B8 || mode->DispInfo.ColorFormat==D3DDDIFMT_A8R8G8B8)) {
                uint8_t *snapshot=(uint8_t *)ExAllocatePoolWithTag(NonPagedPoolNx,4096000u,'cG4A');
                if(snapshot) {
                    for(ULONG row=0;row<1280u;row++)RtlCopyMemory(snapshot+row*3200u,
                        (const uint8_t *)mode->FrameBuffer.Ptr+row*mode->DispInfo.Pitch,3200u);
                    m_DisplayResources.PresentCapture=(const uint32_t *)snapshot;A04DisplayTrace(0x9b,STATUS_SUCCESS);
                } else A04DisplayTrace(0x9b,STATUS_INSUFFICIENT_RESOURCES);
            }
#endif
#if A04_GE8300_WDDM_SHARED_LEASE && !A04_GE8300_WDDM_PRESENT_CAPTURE
            /* Diagnostic-only: the provider admits the lease only after a
             * verified copy/fence. Retry while the copy service is not ready. */
            if (NT_SUCCESS(presentStatus) && m_DisplayResources.Adapter &&
                !m_DisplayResources.Shared.mdl && !m_DisplayResources.GpuHandle
#if A04_GE8300_WDDM_SHARED_COMMON_PAGE
                && a04_common_may_attempt(&m_DisplayResources.SharedCommonOnce)
#endif
                )
                (void)m_DisplayResources.PrepareViaGpu();
#endif
            return presentStatus;
    }

    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::StopDeviceAndReleasePostDisplayOwnership(_In_  D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
                                                                        _Out_ DXGK_DISPLAY_INFORMATION*      pDisplayInfo)
{
    PAGED_CODE();

    if (TargetId >= MAX_CHILDREN || !pDisplayInfo) return STATUS_INVALID_PARAMETER;
#if A04_GE8300_WDDM_SHARED_AUTOSTART
    StopSharedDiagnosticWorker();
#endif
    NTSTATUS status=KeWaitForSingleObject(&m_PresentMutex,Executive,KernelMode,FALSE,NULL);
    if (!NT_SUCCESS(status)) return status;
    A04PresentGuard guard(&m_PresentMutex);
    if (!m_Flags.DriverStarted) return STATUS_DEVICE_NOT_READY;
#if A04_GE8300_WDDM_LIVE_FULL_PRESENT
    if(m_AdapterPowerState==PowerDeviceD0 && m_DisplayResources.LiveFaulted &&
       !NT_SUCCESS(m_DisplayResources.RetireToCpu()))
        return STATUS_IO_DEVICE_ERROR;
#endif
    D3DDDI_VIDEO_PRESENT_SOURCE_ID SourceId = FindSourceForTarget(TargetId, TRUE);
    if (SourceId >= MAX_VIEWS) return STATUS_INVALID_DEVICE_STATE;

    // In case BDD is the next driver to run, the monitor should not be off, since
    // this could cause the BIOS to hang when the EDID is retrieved on Start.
    if (m_MonitorPowerState > PowerDeviceD0)
    {
        SetPowerState(TargetId, PowerDeviceD0, PowerActionNone);
    }

    // The driver has to black out the display and ensure it is visible when releasing ownership
    status=BlackOutScreen(SourceId);
    if(!NT_SUCCESS(status))return status;

    *pDisplayInfo = m_CurrentModes[SourceId].DispInfo;

    m_Flags.DriverStarted = FALSE;
    CleanUp();
    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::QueryVidPnHWCapability(_Inout_ DXGKARG_QUERYVIDPNHWCAPABILITY* pVidPnHWCaps)
{
    PAGED_CODE();

    BDD_ASSERT(pVidPnHWCaps != NULL);
    BDD_ASSERT(pVidPnHWCaps->SourceId < MAX_VIEWS);
    BDD_ASSERT(pVidPnHWCaps->TargetId < MAX_CHILDREN);

    pVidPnHWCaps->VidPnHWCaps.DriverRotation             = A04_GE8300_UNIFIED_DISPLAY356 ? 0 : 1; // unified route has no software rotation
    pVidPnHWCaps->VidPnHWCaps.DriverScaling              = 0; // BDD does not support scaling
    pVidPnHWCaps->VidPnHWCaps.DriverCloning              = 0; // BDD does not support clone
    pVidPnHWCaps->VidPnHWCaps.DriverColorConvert         = A04_GE8300_UNIFIED_DISPLAY356 ? 0 : 1; // no CPU conversion on GPU Present
    pVidPnHWCaps->VidPnHWCaps.DriverLinkedAdapaterOutput = 0; // BDD does not support linked adapters
    pVidPnHWCaps->VidPnHWCaps.DriverRemoteDisplay        = 0; // BDD does not support remote displays

    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::GetEdid(D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId)
{
    PAGED_CODE();

    BDD_ASSERT_CHK(!m_Flags.EDID_Attempted);

    NTSTATUS Status = STATUS_SUCCESS;
    RtlZeroMemory(m_EDIDs[TargetId], sizeof(m_EDIDs[TargetId]));


    m_Flags.EDID_Attempted = TRUE;

    return Status;
}

NTSTATUS BASIC_DISPLAY_DRIVER::BlackOutScreen(D3DDDI_VIDEO_PRESENT_SOURCE_ID SourceId)
{
    if(m_AdapterPowerState!=PowerDeviceD0)return STATUS_DEVICE_NOT_READY;
#if A04_GE8300_WDDM_LIVE_FULL_PRESENT
    if(m_DisplayResources.LiveFaulted && !NT_SUCCESS(m_DisplayResources.RetireToCpu()))
        return STATUS_IO_DEVICE_ERROR;
    m_DisplayResources.LiveShadowValid=FALSE;
#endif
    PAGED_CODE();


    UINT ScreenHeight = m_CurrentModes[SourceId].DispInfo.Height;
    UINT ScreenPitch = m_CurrentModes[SourceId].DispInfo.Pitch;

    PHYSICAL_ADDRESS NewPhysAddrStart = m_CurrentModes[SourceId].DispInfo.PhysicAddress;
    PHYSICAL_ADDRESS NewPhysAddrEnd;
    NewPhysAddrEnd.QuadPart = NewPhysAddrStart.QuadPart + (ScreenHeight * ScreenPitch);

    if (m_CurrentModes[SourceId].Flags.FrameBufferIsActive)
    {
        BYTE* MappedAddr = reinterpret_cast<BYTE*>(m_CurrentModes[SourceId].FrameBuffer.Ptr);

        // Zero any memory at the start that hasn't been zeroed recently
        if (NewPhysAddrStart.QuadPart < m_CurrentModes[SourceId].ZeroedOutStart.QuadPart)
        {
            if (NewPhysAddrEnd.QuadPart < m_CurrentModes[SourceId].ZeroedOutStart.QuadPart)
            {
                // No overlap
                RtlZeroMemory(MappedAddr, ScreenHeight * ScreenPitch);
            }
            else
            {
                RtlZeroMemory(MappedAddr, (UINT)(m_CurrentModes[SourceId].ZeroedOutStart.QuadPart - NewPhysAddrStart.QuadPart));
            }
        }

        // Zero any memory at the end that hasn't been zeroed recently
        if (NewPhysAddrEnd.QuadPart > m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart)
        {
            if (NewPhysAddrStart.QuadPart > m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart)
            {
                // No overlap
                // NOTE: When actual pixels were the most recent thing drawn, ZeroedOutStart & ZeroedOutEnd will both be 0
                // and this is the path that will be used to black out the current screen.
                RtlZeroMemory(MappedAddr, ScreenHeight * ScreenPitch);
            }
            else
            {
                RtlZeroMemory(MappedAddr, (UINT)(NewPhysAddrEnd.QuadPart - m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart));
            }
        }
    }

    m_CurrentModes[SourceId].ZeroedOutStart.QuadPart = NewPhysAddrStart.QuadPart;
    m_CurrentModes[SourceId].ZeroedOutEnd.QuadPart = NewPhysAddrEnd.QuadPart;
    return STATUS_SUCCESS;
}

NTSTATUS BASIC_DISPLAY_DRIVER::WriteHWInfoStr(_In_ HANDLE DevInstRegKeyHandle, _In_ PCWSTR pszwValueName, _In_ PCSTR pszValue)
{
    PAGED_CODE();

    NTSTATUS Status;
    ANSI_STRING AnsiStrValue;
    UNICODE_STRING UnicodeStrValue;
    UNICODE_STRING UnicodeStrValueName;

    // ZwSetValueKey wants the ValueName as a UNICODE_STRING
    RtlInitUnicodeString(&UnicodeStrValueName, pszwValueName);

    // REG_SZ is for WCHARs, there is no equivalent for CHARs
    // Use the ansi/unicode conversion functions to get from PSTR to PWSTR
    RtlInitAnsiString(&AnsiStrValue, pszValue);
    Status = RtlAnsiStringToUnicodeString(&UnicodeStrValue, &AnsiStrValue, TRUE);
    if (!NT_SUCCESS(Status))
    {
        BDD_LOG_ERROR1("RtlAnsiStringToUnicodeString failed with Status: 0x%I64x", Status);
        return Status;
    }

    // Write the value to the registry
    Status = ZwSetValueKey(DevInstRegKeyHandle,
                           &UnicodeStrValueName,
                           0,
                           REG_SZ,
                           UnicodeStrValue.Buffer,
                           UnicodeStrValue.MaximumLength);

    // Free the earlier allocated unicode string
    RtlFreeUnicodeString(&UnicodeStrValue);

    if (!NT_SUCCESS(Status))
    {
        BDD_LOG_ERROR1("ZwSetValueKey failed with Status: 0x%I64x", Status);
    }

    return Status;
}

NTSTATUS BASIC_DISPLAY_DRIVER::RegisterHWInfo()
{
    PAGED_CODE();

    NTSTATUS Status;

    // The display pipeline is the MT8167 controller; GE8300 is a separate
    // optional copy provider and must not be identified as this display adapter.
    PCSTR StrHWInfoChipType = "MediaTek MT8167";
    PCSTR StrHWInfoDacType = "MIPI DSI";
    PCSTR StrHWInfoAdapterString = "MediaTek MT8167 Display Controller";
    PCSTR StrHWInfoBiosString = "UEFI";

    HANDLE DevInstRegKeyHandle;
    UNICODE_STRING ValueNameMemorySize;
    DWORD MemorySize = 0; // BDD has no access to video memory
    Status = IoOpenDeviceRegistryKey(m_pPhysicalDevice, PLUGPLAY_REGKEY_DRIVER, KEY_SET_VALUE, &DevInstRegKeyHandle);
    if (!NT_SUCCESS(Status))
    {
        BDD_LOG_ERROR2("IoOpenDeviceRegistryKey failed for PDO: 0x%I64x, Status: 0x%I64x", m_pPhysicalDevice, Status);
        return Status;
    }

    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.ChipType", StrHWInfoChipType);
    if (!NT_SUCCESS(Status))
    {
        goto CloseRegistryKey;
    }

    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.DacType", StrHWInfoDacType);
    if (!NT_SUCCESS(Status))
    {
        goto CloseRegistryKey;
    }

    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.AdapterString", StrHWInfoAdapterString);
    if (!NT_SUCCESS(Status))
    {
        goto CloseRegistryKey;
    }

    Status = WriteHWInfoStr(DevInstRegKeyHandle, L"HardwareInformation.BiosString", StrHWInfoBiosString);
    if (!NT_SUCCESS(Status))
    {
        goto CloseRegistryKey;
    }

    // MemorySize is a ULONG, unlike the others which are all strings
    RtlInitUnicodeString(&ValueNameMemorySize, L"HardwareInformation.MemorySize");
    Status = ZwSetValueKey(DevInstRegKeyHandle,
                           &ValueNameMemorySize,
                           0,
                           REG_DWORD,
                           &MemorySize,
                           sizeof(MemorySize));
    if (!NT_SUCCESS(Status))
    {
        BDD_LOG_ERROR1("ZwSetValueKey for MemorySize failed with Status: 0x%I64x", Status);
    }

CloseRegistryKey:
    ZwClose(DevInstRegKeyHandle);
    return Status;
}

//
// Non-Paged Code
//
#pragma code_seg(push)
#pragma code_seg()
D3DDDI_VIDEO_PRESENT_SOURCE_ID BASIC_DISPLAY_DRIVER::FindSourceForTarget(D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId, BOOLEAN DefaultToZero)
{
    UNREFERENCED_PARAMETER(TargetId);
    BDD_ASSERT_CHK(TargetId < MAX_CHILDREN);

    for (UINT SourceId = 0; SourceId < MAX_VIEWS; ++SourceId)
    {
        if (m_CurrentModes[SourceId].FrameBuffer.Ptr != NULL)
        {
            return SourceId;
        }
    }

    return DefaultToZero ? 0 : D3DDDI_ID_UNINITIALIZED;
}

VOID BASIC_DISPLAY_DRIVER::DpcRoutine(VOID)
{
    m_DxgkInterface.DxgkCbNotifyDpc((HANDLE)m_DxgkInterface.DeviceHandle);
}

BOOLEAN BASIC_DISPLAY_DRIVER::InterruptRoutine(_In_  ULONG MessageNumber)
{
    UNREFERENCED_PARAMETER(MessageNumber);

    // BDD cannot handle interrupts
    return FALSE;
}

VOID BASIC_DISPLAY_DRIVER::ResetDevice(VOID)
{
}

// Must be Non-Paged, as it sets up the display for a bugcheck
NTSTATUS BASIC_DISPLAY_DRIVER::SystemDisplayEnable(_In_  D3DDDI_VIDEO_PRESENT_TARGET_ID TargetId,
                                                   _In_  PDXGKARG_SYSTEM_DISPLAY_ENABLE_FLAGS Flags,
                                                   _Out_ UINT* pWidth,
                                                   _Out_ UINT* pHeight,
                                                   _Out_ D3DDDIFORMAT* pColorFormat)
{
    UNREFERENCED_PARAMETER(Flags);

    m_SystemDisplaySourceId = D3DDDI_ID_UNINITIALIZED;

    BDD_ASSERT((TargetId < MAX_CHILDREN) || (TargetId == D3DDDI_ID_UNINITIALIZED));

    // Find the frame buffer for displaying the bugcheck, if it was successfully mapped
    if (TargetId == D3DDDI_ID_UNINITIALIZED)
    {
        for (UINT SourceIdx = 0; SourceIdx < MAX_VIEWS; ++SourceIdx)
        {
            if (m_CurrentModes[SourceIdx].FrameBuffer.Ptr != NULL)
            {
                m_SystemDisplaySourceId = SourceIdx;
                break;
            }
        }
    }
    else
    {
        m_SystemDisplaySourceId = FindSourceForTarget(TargetId, FALSE);
    }

    if (m_SystemDisplaySourceId == D3DDDI_ID_UNINITIALIZED)
    {
        {
            return STATUS_UNSUCCESSFUL;
        }
    }

    if ((m_CurrentModes[m_SystemDisplaySourceId].Rotation == D3DKMDT_VPPR_ROTATE90) ||
        (m_CurrentModes[m_SystemDisplaySourceId].Rotation == D3DKMDT_VPPR_ROTATE270))
    {
        *pHeight = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Width;
        *pWidth = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Height;
    }
    else
    {
        *pWidth = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Width;
        *pHeight = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Height;
    }

    *pColorFormat = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.ColorFormat;


    return STATUS_SUCCESS;
}

// Must be Non-Paged, as it is called to display the bugcheck screen
VOID BASIC_DISPLAY_DRIVER::SystemDisplayWrite(_In_reads_bytes_(SourceHeight * SourceStride) VOID* pSource,
                                              _In_ UINT SourceWidth,
                                              _In_ UINT SourceHeight,
                                              _In_ UINT SourceStride,
                                              _In_ INT PositionX,
                                              _In_ INT PositionY)
{

    if(m_SystemDisplaySourceId>=MAX_VIEWS)return;
    const CURRENT_BDD_MODE& mode=m_CurrentModes[m_SystemDisplaySourceId];
    unsigned turns;uint32_t width,height;size_t allocation;uint32_t span;
    if(!pSource || !mode.FrameBuffer.Ptr || a04_present_turns((unsigned)mode.Rotation,&turns))return;
    width=(turns&1u)?mode.DispInfo.Height:mode.DispInfo.Width;
    height=(turns&1u)?mode.DispInfo.Width:mode.DispInfo.Height;
    if(PositionX<0 || PositionY<0 || !SourceWidth || !SourceHeight ||
       (uint64_t)(UINT)PositionX+SourceWidth>width || (uint64_t)(UINT)PositionY+SourceHeight>height ||
       a04_present_bounds(SourceWidth,SourceHeight,4,(int32_t)SourceStride,0,0,0,0,0,&allocation,&span) ||
       (uintptr_t)pSource>UINTPTR_MAX-span)return;
    // Rect will be Offset by PositionX/Y in the src to reset it back to 0
    RECT Rect;
    Rect.left = PositionX;
    Rect.top = PositionY;
    Rect.right =  Rect.left + SourceWidth;
    Rect.bottom = Rect.top + SourceHeight;

    // Set up destination blt info
    BLT_INFO DstBltInfo;
    DstBltInfo.pBits = m_CurrentModes[m_SystemDisplaySourceId].FrameBuffer.Ptr;
    DstBltInfo.Pitch = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Pitch;
    DstBltInfo.BitsPerPel = BPPFromPixelFormat(m_CurrentModes[m_SystemDisplaySourceId].DispInfo.ColorFormat);
    DstBltInfo.Offset.x = 0;
    DstBltInfo.Offset.y = 0;
    DstBltInfo.Rotation = m_CurrentModes[m_SystemDisplaySourceId].Rotation;
    DstBltInfo.Width = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Width;
    DstBltInfo.Height = m_CurrentModes[m_SystemDisplaySourceId].DispInfo.Height;

    // Set up source blt info
    BLT_INFO SrcBltInfo;
    SrcBltInfo.pBits = pSource;
    SrcBltInfo.Pitch = SourceStride;
    SrcBltInfo.BitsPerPel = 32;

    SrcBltInfo.Offset.x = -PositionX;
    SrcBltInfo.Offset.y = -PositionY;
    SrcBltInfo.Rotation = D3DKMDT_VPPR_IDENTITY;
    SrcBltInfo.Width = SourceWidth;
    SrcBltInfo.Height = SourceHeight;

    BltBits(&DstBltInfo,
            &SrcBltInfo,
            1, // NumRects
            &Rect);
}

#pragma code_seg(pop) // End Non-Paged Code

NTSTATUS BASIC_DISPLAY_DRIVER::QueryBrightness(PQUERY_INTERFACE query)
{
    PAGED_CODE();
#if A04_MT8167_DISPLAY_BRIGHTNESS
    NTSTATUS status=KeWaitForSingleObject(&m_PresentMutex,Executive,KernelMode,FALSE,NULL);
    if(!NT_SUCCESS(status))return status;
    A04PresentGuard guard(&m_PresentMutex);
    return m_Flags.DriverStarted ? m_Brightness.QueryInterface(query) : STATUS_DEVICE_NOT_READY;
#else
    UNREFERENCED_PARAMETER(query);return STATUS_NOT_SUPPORTED;
#endif
}
