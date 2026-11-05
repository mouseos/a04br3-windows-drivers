#!/usr/bin/env python3
"""Validate the GPST IOCTL contract against a mocked ACPI device."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/gpu-power-acpi-host'
out.mkdir(parents=True,exist_ok=True)
base=(root/'tests/gpu_windows_stubs/ntddk.h').read_text()
extra=r'''
#ifndef A04_TEST_WDF_TYPES
#define A04_TEST_WDF_TYPES
typedef void *WDFDEVICE;
#define FIELD_OFFSET(t,f) ((ULONG)offsetof(t,f))
typedef struct {void *buffer;size_t length;} WDF_MEMORY_DESCRIPTOR;
typedef struct {int flags;int64_t timeout;} WDF_REQUEST_SEND_OPTIONS;
#define WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(d,p,n) do{(d)->buffer=(void *)(p);(d)->length=(n);}while(0)
#define WDF_REQUEST_SEND_OPTION_TIMEOUT 1
#define WDF_REQUEST_SEND_OPTIONS_INIT(o,f) do{(o)->flags=(f);}while(0)
#define WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(o,t) do{(o)->timeout=(t);}while(0)
#define WDF_REL_TIMEOUT_IN_MS(x) (-(int64_t)(x)*10000)
#define WdfDeviceGetIoTarget(d) (d)
NTSTATUS WdfIoTargetSendIoctlSynchronously(void *,void *,ULONG,WDF_MEMORY_DESCRIPTOR *,WDF_MEMORY_DESCRIPTOR *,WDF_REQUEST_SEND_OPTIONS *,ULONG_PTR *);
#endif
'''
(out/'ntddk.h').write_text(base+'\n'+extra)
(out/'wdf.h').write_text('#include <ntddk.h>\n')
(out/'acpiioct.h').write_text(r'''
#include <ntddk.h>
#define ACPI_EVAL_INPUT_BUFFER_SIGNATURE 0x12345678u
#define ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE 0x87654321u
#define ACPI_METHOD_ARGUMENT_BUFFER 1
#define IOCTL_ACPI_EVAL_METHOD 0x1337u
typedef struct {ULONG Signature;ULONG MethodNameAsUlong;} ACPI_EVAL_INPUT_BUFFER;
typedef struct {uint16_t Type,DataLength;union {ULONG Argument;UCHAR Data[1];};} ACPI_METHOD_ARGUMENT;
typedef struct {ULONG Signature,Length,Count;ACPI_METHOD_ARGUMENT Argument[1];} ACPI_EVAL_OUTPUT_BUFFER,*PACPI_EVAL_OUTPUT_BUFFER;
''')
(out/'test.c').write_text(r'''
#include <assert.h>
#include <ntddk.h>
#include <acpiioct.h>
#include "windows_power_acpi.h"
static int mode;
unsigned KeGetCurrentIrql(void){return PASSIVE_LEVEL;}
static void put(UCHAR *p,ULONG x){unsigned i;for(i=0;i<4;i++)p[i]=(UCHAR)(x>>(8*i));}
NTSTATUS WdfIoTargetSendIoctlSynchronously(void *target,void *request,ULONG code,WDF_MEMORY_DESCRIPTOR *in,WDF_MEMORY_DESCRIPTOR *out,WDF_REQUEST_SEND_OPTIONS *opt,ULONG_PTR *ret){
 ACPI_EVAL_INPUT_BUFFER *input=in->buffer;ACPI_EVAL_OUTPUT_BUFFER *o=out->buffer;
 ULONG n=FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument)+FIELD_OFFSET(ACPI_METHOD_ARGUMENT,Data);UCHAR *p=o->Argument[0].Data;
 assert(target==(void*)1&&!request&&code==IOCTL_ACPI_EVAL_METHOD&&input->MethodNameAsUlong==0x54535047u&&opt->timeout==WDF_REL_TIMEOUT_IN_MS(2000));
 if(mode==1)return STATUS_IO_TIMEOUT;
 o->Signature=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE;o->Count=1;o->Length=n+20;o->Argument[0].Type=ACPI_METHOD_ARGUMENT_BUFFER;o->Argument[0].DataLength=20;
 put(p,(1u<<25)|(1u<<24)|(1u<<4));put(p+4,(1u<<25)|(1u<<24)|(1u<<4));put(p+8,4u<<8);put(p+12,8u<<8);put(p+16,0);*ret=o->Length;
 if(mode==2)*ret=n-1;if(mode==3)o->Argument[0].DataLength=19;if(mode==4)o->Count=2;if(mode==5)put(p+4,0);
 return STATUS_SUCCESS;
}
int main(void){struct a04_vendor_power_snapshot p;ULONG mfg=0;int i;
 for(mode=0;mode<=5;mode++){
  volatile UCHAR *address=mode==5?(volatile UCHAR *)(uintptr_t)0x1000:(volatile UCHAR *)&mfg;
  NTSTATUS status=a04_vendor_m1_11_power_snapshot((void*)1,address,4,&p);
  if(mode==0){assert(status==STATUS_SUCCESS&&p.top_mux0==(4u<<8)&&p.top_mux1==(8u<<8)&&p.mfg_clock_status==0);}
  if(mode==1)assert(status==STATUS_IO_TIMEOUT);
  if(mode>=2&&mode<=4)assert(status==STATUS_DATA_ERROR);
  if(mode==5)assert(status==STATUS_DEVICE_NOT_READY);
  if(mode){const UCHAR *bytes=(const UCHAR *)&p;for(i=0;i<(int)sizeof(p);i++)assert(bytes[i]==0);}
 }
 return 0;}
''')
src=root/'drivers/gpu/mt8167/vendor_m1_11'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(out),'-I'+str(src),str(src/'windows_power_acpi.c'),str(out/'test.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
print('GE8300 GPST ACPI response validation: PASS (mock)')
