#!/usr/bin/env python3
"""Actual initialization recorder with DriverObject/registry mocks; no hardware."""
import argparse,subprocess,json,hashlib,importlib.util
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(r) or out.exists():raise ValueError('New checkout-local output required')
out.mkdir(parents=True);s=(r/'drivers/gpu/mt8167/wddm-render/miniport.c').read_text();start=s.index('struct geo_module_identity_v1 {');end=s.index('NTSTATUS DriverEntry(',start);actual=s[start:end]
code=r'''#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
typedef uint32_t ULONG;typedef uint64_t ULONGLONG;typedef uintptr_t ULONG_PTR;typedef void*PVOID;typedef char CHAR;typedef void*HANDLE;typedef int NTSTATUS;typedef struct{int64_t QuadPart;}LARGE_INTEGER;typedef struct{void*DriverStart;ULONG DriverSize;}DRIVER_OBJECT,*PDRIVER_OBJECT;typedef struct{int x;}UNICODE_STRING,OBJECT_ATTRIBUTES;
#define PASSIVE_LEVEL 0
#define C_ASSERT(x) _Static_assert(x,"record ABI")
#define RtlCopyMemory memcpy
#define OBJ_KERNEL_HANDLE 1
#define OBJ_CASE_INSENSITIVE 2
#define KEY_SET_VALUE 4
#define REG_OPTION_NON_VOLATILE 0
#define REG_BINARY 3
#define NT_SUCCESS(x) ((x)>=0)
static unsigned irql,created,set,flushed,closed,mode;static unsigned char record[144];
static ULONG KeGetCurrentIrql(void){return irql;}
static void KeQuerySystemTime(LARGE_INTEGER*t){t->QuadPart=134000000000000000LL;}
static ULONGLONG KeQueryInterruptTime(void){return 123456789;}
#define RtlInitUnicodeString(p,s) ((void)(p),(void)(s))
#define InitializeObjectAttributes(o,p,f,r,s) ((void)(o),(void)(p),(void)(f),(void)(r),(void)(s))
static NTSTATUS ZwCreateKey(HANDLE*k,ULONG access,OBJECT_ATTRIBUTES*o,int t,void*x,ULONG f,ULONG*d){(void)access;(void)o;(void)t;(void)x;(void)f;(void)d;created++;if(mode==1)return -1;*k=(void*)1;return 0;}
static NTSTATUS ZwSetValueKey(HANDLE k,UNICODE_STRING*n,int t,ULONG type,void*d,ULONG bytes){(void)k;(void)n;(void)t;assert(type==3&&bytes==144);set++;memcpy(record,d,bytes);return mode==2?-1:0;}
static NTSTATUS ZwFlushKey(HANDLE k){(void)k;flushed++;return mode==3?-1:0;}
static void ZwClose(HANDLE k){(void)k;closed++;}
'''+actual+r'''
int main(int argc,char**argv){assert(argc==2);DRIVER_OBJECT driver={(void*)(uintptr_t)0x8b120000u,0x38000};unsigned cases=0;
 for(mode=0;mode<4;mode++){created=set=flushed=closed=0;irql=0;module_identity(&driver);assert(created==1);assert(set==(mode!=1));assert(flushed==(mode==0||mode==3));assert(closed==(mode!=1));cases++;}
 irql=2;created=0;module_identity(&driver);assert(!created);cases++;
 irql=0;module_identity(NULL);assert(!created);cases++;
 DRIVER_OBJECT invalid={NULL,0x38000};module_identity(&invalid);assert(!created);cases++;
 invalid.DriverStart=driver.DriverStart;invalid.DriverSize=0;module_identity(&invalid);assert(!created);cases++;
 struct geo_module_identity_v1*r=(void*)record;assert(r->driver_start==0x8b120000&&r->driver_size==0x38000&&r->pointer_bits==32&&r->schema==1&&r->bytes==144);cases++;
 FILE*f=fopen(argv[1],"wb");assert(f);assert(fwrite(record,1,sizeof(record),f)==sizeof(record));fclose(f);
 printf("module identity recorder: %u cases passed; actual function; mocked registry/DriverObject\n",cases);return 0;}
'''
# Host pointer width is64; production module is ARM32. Set recorder's pointer-bit
# expression to32 only in this mock ABI fixture, leaving all control flow intact.
code=code.replace('r.pointer_bits=sizeof(PVOID)*8;','r.pointer_bits=32;')
f=out/'actual-recorder.c';f.write_text(code)
x=subprocess.run(['clang','-std=c11','-fshort-wchar','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(f),'-o',str(out/'test')],capture_output=True,text=True);(out/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode()
x=subprocess.run([str(out/'test'),str(out/'record.bin')],capture_output=True,text=True);(out/'test.log').write_text(x.stdout+x.stderr);x.check_returncode();print(x.stdout,end='')
spec=importlib.util.spec_from_file_location('decoder',r/'tools/ci/read-ge8300-module-identity.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);result=m.decode((out/'record.bin').read_bytes());assert result['same_boot_PC_correlation_proven'] is False
for bad in [b'',(out/'record.bin').read_bytes()[:-1],b'\0'*144]:
 try:m.decode(bad);raise AssertionError('Malformed record accepted')
 except ValueError:pass
(out/'receipt.json').write_text(json.dumps({'actual_recorder_cases':9,'decoder_cases':4,'ARM_pointer_width_mocked_on_host':True,'ASAN_UBSAN':True,'source_sha256':hashlib.sha256((r/'drivers/gpu/mt8167/wddm-render/miniport.c').read_bytes()).hexdigest(),'same_boot_correlation_proven':False,'hardware_operations':False},indent=2)+'\n')
