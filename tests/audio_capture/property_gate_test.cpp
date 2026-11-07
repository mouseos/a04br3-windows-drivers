/* Compile the actual METHOD_NEITHER gate with a safe-copy DDI fixture. */
#include "portcls.h"
#include <assert.h>
extern "C" NTSTATUS Mt8167LatencyPropertyGate(PIRP,BOOLEAN*);
static unsigned irql,calls;static NTSTATUS result;static size_t limit=sizeof(KSPROPERTY);
unsigned KeGetCurrentIrql(){return irql;}
NTSTATUS MmCopyMemory(void*d,MM_COPY_ADDRESS s,SIZE_T n,unsigned flags,SIZE_T*copied){
 ++calls;assert(n==sizeof(KSPROPERTY)&&flags==MM_COPY_MEMORY_VIRTUAL);
 *copied=0;if(result<0)return result;assert(s.VirtualAddress);
 *copied=limit<n?limit:n;memcpy(d,s.VirtualAddress,*copied);return result;
}
int main(){IRP irp={};BOOLEAN handled=TRUE;KSPROPERTY p={KSPROPSETID_RtAudio,KSPROPERTY_RTAUDIO_HWLATENCY,1};
 irp.stack.Parameters.DeviceIoControl.Type3InputBuffer=&p;
 assert(Mt8167LatencyPropertyGate(&irp,&handled)==0&&!handled&&!calls);
 irp.stack.Parameters.DeviceIoControl.IoControlCode=IOCTL_KS_PROPERTY;
 irp.stack.Parameters.DeviceIoControl.InputBufferLength=sizeof(p)-1;
 assert(Mt8167LatencyPropertyGate(&irp,&handled)==0&&!handled&&!calls);
 irp.stack.Parameters.DeviceIoControl.InputBufferLength=sizeof(p);
 irql=2;assert(Mt8167LatencyPropertyGate(&irp,&handled)==STATUS_INVALID_DEVICE_STATE&&handled&&!calls);irql=0;
 /* Invalid/unmapped user pointer is never dereferenced by the driver. */
 result=STATUS_PARTIAL_COPY;irp.stack.Parameters.DeviceIoControl.Type3InputBuffer=(void*)1;
 assert(Mt8167LatencyPropertyGate(&irp,&handled)==STATUS_PARTIAL_COPY&&handled&&calls==1);
 result=0;irp.stack.Parameters.DeviceIoControl.Type3InputBuffer=&p;limit=sizeof(p)-1;
 assert(Mt8167LatencyPropertyGate(&irp,&handled)==STATUS_INVALID_USER_BUFFER&&handled);limit=sizeof(p);
 assert(Mt8167LatencyPropertyGate(&irp,&handled)==STATUS_NOT_SUPPORTED&&handled);
 p.Id++;assert(Mt8167LatencyPropertyGate(&irp,&handled)==0&&!handled);
 p.Id--;p.Set.a++;assert(Mt8167LatencyPropertyGate(&irp,&handled)==0&&!handled);
 return 0;
}
