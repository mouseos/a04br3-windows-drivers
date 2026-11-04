#!/usr/bin/env python3
"""Exercise actual Windows BQHR IOCTL path with mocked WDF/ACPI, no hardware."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
mock=r'''
#ifndef MOCK_NT_H
#define MOCK_NT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint32_t ULONG; typedef uintptr_t ULONG_PTR; typedef uint64_t ULONGLONG;
typedef uint8_t UCHAR; typedef int32_t NTSTATUS; typedef void *WDFDEVICE;
#define NT_SUCCESS(s) ((s)>=0)
#define FIELD_OFFSET(t,f) ((ULONG)offsetof(t,f))
#define RtlZeroMemory(p,n) memset(p,0,n)
typedef struct { void *buffer; size_t length; } WDF_MEMORY_DESCRIPTOR;
typedef struct { int flags; int64_t timeout; } WDF_REQUEST_SEND_OPTIONS;
#define WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(d,p,n) do{(d)->buffer=(p);(d)->length=(n);}while(0)
#define WDF_REQUEST_SEND_OPTION_TIMEOUT 1
#define WDF_REQUEST_SEND_OPTIONS_INIT(o,f) do{(o)->flags=(f);}while(0)
#define WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(o,t) do{(o)->timeout=(t);}while(0)
#define WDF_REL_TIMEOUT_IN_MS(x) (-(int64_t)(x)*10000)
#define WdfDeviceGetIoTarget(d) (d)
NTSTATUS WdfIoTargetSendIoctlSynchronously(void *,void *,ULONG,WDF_MEMORY_DESCRIPTOR *,WDF_MEMORY_DESCRIPTOR *,WDF_REQUEST_SEND_OPTIONS *,ULONG_PTR *);
#endif
'''
acpi=r'''
#include <ntddk.h>
#define ACPI_EVAL_INPUT_BUFFER_SIGNATURE 0x12345678u
#define ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE 0x87654321u
#define ACPI_METHOD_ARGUMENT_BUFFER 1
#define IOCTL_ACPI_EVAL_METHOD 0x1337u
typedef struct {ULONG Signature; ULONG MethodNameAsUlong;} ACPI_EVAL_INPUT_BUFFER;
typedef struct {uint16_t Type,DataLength;union {ULONG Argument; UCHAR Data[1];};} ACPI_METHOD_ARGUMENT;
typedef struct {ULONG Signature,Length,Count;ACPI_METHOD_ARGUMENT Argument[1];} ACPI_EVAL_OUTPUT_BUFFER,*PACPI_EVAL_OUTPUT_BUFFER;
'''
harness=r'''
#include <assert.h>
#include <ntddk.h>
#include <acpiioct.h>
#include "bqhr_acpi.h"
static int mode;
static void put(UCHAR *p,uint64_t x){unsigned i;for(i=0;i<8;++i)p[i]=(UCHAR)(x>>(i*8));}
static void seal(UCHAR *p){uint64_t h=14695981039346656037ULL;unsigned i;for(i=0;i<120;++i){h^=p[i];h*=1099511628211ULL;}put(p+120,h);}
NTSTATUS WdfIoTargetSendIoctlSynchronously(void *target,void *request,ULONG code,WDF_MEMORY_DESCRIPTOR *in,WDF_MEMORY_DESCRIPTOR *out,WDF_REQUEST_SEND_OPTIONS *opt,ULONG_PTR *ret){
 ACPI_EVAL_INPUT_BUFFER *input=in->buffer;ACPI_EVAL_OUTPUT_BUFFER *o=out->buffer;UCHAR *p=o->Argument[0].Data;ULONG off=FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument)+FIELD_OFFSET(ACPI_METHOD_ARGUMENT,Data);
 assert(target==(void*)1&&!request&&code==IOCTL_ACPI_EVAL_METHOD&&input->Signature==ACPI_EVAL_INPUT_BUFFER_SIGNATURE&&input->MethodNameAsUlong==0x52485142u);
 assert(opt->flags==1&&opt->timeout==WDF_REL_TIMEOUT_IN_MS(2000));if(mode==1)return -1;
 o->Signature=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE;o->Count=1;o->Length=off+128;o->Argument[0].Type=ACPI_METHOD_ARGUMENT_BUFFER;o->Argument[0].DataLength=128;
 put(p,0x314f524147484334ULL);put(p+8,128);put(p+16,2);put(p+96,6);seal(p);*ret=o->Length;
 if(mode==2)*ret=off-1; if(mode==3)o->Argument[0].DataLength=127;if(mode==4)o->Length=off+127;
 if(mode==5)p[0]^=1;if(mode==6){put(p+16,1);seal(p);}if(mode==7)o->Count=2;
 if(mode==8)*ret=out->length+1;return 0;
}
int main(void){NTSTATUS status;enum a04_bqhr_acpi_result expected[]={A04_BQHR_ACPI_STOPPED,A04_BQHR_ACPI_IOCTL_ERROR,A04_BQHR_ACPI_INVALID_OUTPUT,A04_BQHR_ACPI_INVALID_OUTPUT,A04_BQHR_ACPI_INVALID_OUTPUT,A04_BQHR_ACPI_INVALID_RECORD,A04_BQHR_ACPI_OTHER_STATE,A04_BQHR_ACPI_INVALID_OUTPUT,A04_BQHR_ACPI_INVALID_OUTPUT};
 for(mode=0;mode<9;++mode)assert(a04_bqhr_evaluate((void*)1,&status)==expected[mode]);return 0;}
'''
with tempfile.TemporaryDirectory(prefix='a04-bqhr-wdf-') as td:
 p=Path(td);(p/'ntddk.h').write_text(mock);(p/'wdf.h').write_text('#include <ntddk.h>\n');(p/'acpiioct.h').write_text(acpi);(p/'test.c').write_text(harness)
 src=root/'drivers/power/bq24160'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(p),'-I'+str(src),str(src/'bqhr_acpi.c'),str(src/'bqhr_diagnostic.c'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('BQHR ACPI IOCTL, length/type checks and non-adoption: PASS (host mock)')
