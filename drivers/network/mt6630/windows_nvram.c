/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_nvram.h"
#include "windows_nvram_reader.h"
#define A04_NVRAM_TAG 'v40A'
static VOID a04_nvram_worker(PVOID context,NDIS_HANDLE handle)
{
 struct a04_windows_nvram_job *job=(struct a04_windows_nvram_job *)context;
 UCHAR *result;NTSTATUS status;UNREFERENCED_PARAMETER(handle);
 result=ExAllocatePoolWithTag(PagedPool,512,A04_NVRAM_TAG);
 status=result?a04_windows_nvram_read(result):STATUS_INSUFFICIENT_RESOURCES;
 job->complete(job->context,status,NT_SUCCESS(status)?result:NULL);
 if(result){RtlSecureZeroMemory(result,512);ExFreePoolWithTag(result,A04_NVRAM_TAG);}
 NdisFreeIoWorkItem(handle);
 KeSetEvent(&job->done,IO_NO_INCREMENT,FALSE);
}
NDIS_STATUS a04_windows_nvram_init(struct a04_windows_nvram_job *job,NDIS_HANDLE adapter,a04_nvram_completion callback,void *context)
{
 if(!job||!adapter||!callback)return NDIS_STATUS_INVALID_DATA;
 RtlZeroMemory(job,sizeof(*job));KeInitializeEvent(&job->done,NotificationEvent,FALSE);
 job->complete=callback;job->context=context;job->work=NdisAllocateIoWorkItem(adapter);
 return job->work?NDIS_STATUS_SUCCESS:NDIS_STATUS_RESOURCES;
}
NDIS_STATUS a04_windows_nvram_start(struct a04_windows_nvram_job *job)
{
 if(!job||!job->work||InterlockedCompareExchange(&job->started,1,0))return NDIS_STATUS_INVALID_DATA;
 NdisQueueIoWorkItem(job->work,a04_nvram_worker,job);return NDIS_STATUS_PENDING;
}
void a04_windows_nvram_drain(struct a04_windows_nvram_job *job)
{
 if(!job||!job->work)return;
 if(job->started)KeWaitForSingleObject(&job->done,Executive,KernelMode,FALSE,NULL);
 else NdisFreeIoWorkItem(job->work);
 job->work=NULL;
}
