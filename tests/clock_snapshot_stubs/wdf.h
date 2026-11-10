#ifndef CLOCK_TEST_WDF_H
#define CLOCK_TEST_WDF_H
#include "ntddk.h"
typedef void *WDFDEVICE;
typedef void *WDFQUEUE;typedef void *WDFREQUEST;
NTSTATUS WdfFdoQueryForInterface(WDFDEVICE,const GUID *,PINTERFACE,ULONG,ULONG,PVOID);
WDFDEVICE WdfIoQueueGetDevice(WDFQUEUE);
NTSTATUS WdfRequestRetrieveOutputBuffer(WDFREQUEST,size_t,PVOID *,size_t *);
VOID WdfRequestComplete(WDFREQUEST,NTSTATUS);
VOID WdfRequestCompleteWithInformation(WDFREQUEST,NTSTATUS,size_t);
#endif
