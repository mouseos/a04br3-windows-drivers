/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Included only by the existing PASSIVE, sequential thermal1.10 child.
 * Does not touch first-read state or call thermal Measure/Acquire. */
static VOID Mt8167ClockSnapshotIoctl(WDFQUEUE queue,WDFREQUEST request,
                                    size_t outputSize,size_t inputSize)
{
    struct mt8167_clock_snapshot *buffer;NTSTATUS status;
    if(inputSize || outputSize!=sizeof(*buffer)){
        WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;
    }
    status=WdfRequestRetrieveOutputBuffer(request,sizeof(*buffer),(PVOID *)&buffer,NULL);
    if(!NT_SUCCESS(status)){WdfRequestComplete(request,status);return;}
    status=Mt8167ReadParentClockSnapshot(WdfIoQueueGetDevice(queue),buffer);
    WdfRequestCompleteWithInformation(request,status,NT_SUCCESS(status)?sizeof(*buffer):0);
}
