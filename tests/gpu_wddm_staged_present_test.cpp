/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cstddef>
#define _BDD_HXX_
#define _In_
#define A04_GE8300_WDDM_COPY 1
using BYTE=unsigned char; using ULONG=uint32_t; using SIZE_T=size_t; using NTSTATUS=int32_t;
using HANDLE=void*; using PFILE_OBJECT=void*; using PVOID=void*;
struct RECT { int32_t left,top,right,bottom; };
struct BLT_INFO { BYTE *pBits; uint32_t Pitch,BitsPerPel; struct { int x,y; } Offset; int Rotation; uint32_t Width,Height; };
struct IO_STATUS_BLOCK { NTSTATUS Status; SIZE_T Information; };
struct OBJECT_ATTRIBUTES {};
struct UNICODE_STRING { const wchar_t *p; };
#define RTL_CONSTANT_STRING(x) {x}
#define PASSIVE_LEVEL 0
#define D3DKMDT_VPPR_IDENTITY 0
#define FILE_READ_DATA 1
#define FILE_WRITE_DATA 2
#define SYNCHRONIZE 4
#define OBJ_KERNEL_HANDLE 1
#define OBJ_CASE_INSENSITIVE 2
#define FILE_ATTRIBUTE_NORMAL 0
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define FILE_OPEN 1
#define FILE_NON_DIRECTORY_FILE 1
#define FILE_SYNCHRONOUS_IO_NONALERT 2
#define KernelMode 0
#define Executive 0
#define FALSE 0
#define TRUE 1
#define BOOLEAN int
#define STATUS_PENDING 0x103
#define STATUS_IO_DEVICE_ERROR ((NTSTATUS)-1)
#define NT_SUCCESS(x) ((x)>=0)
#define NonPagedPoolNx 0
static int scenario, calls, writes, allocs; static BYTE *fb; static size_t fbsize; static void *object_type;
static void **IoFileObjectType=&object_type;
static int KeGetCurrentIrql(){return 0;} static int KeAreAllApcsDisabled(){return 0;}
static void InitializeObjectAttributes(OBJECT_ATTRIBUTES*,UNICODE_STRING*,int,void*,void*){}
static NTSTATUS ZwCreateFile(HANDLE *h,int,OBJECT_ATTRIBUTES*,IO_STATUS_BLOCK*,void*,int,int,int,int,void*,int){*h=(void*)1;return 0;}
static NTSTATUS ObReferenceObjectByHandle(HANDLE,int,void*,int,PVOID *o,void*){*o=(void*)1;return 0;}
static void ObDereferenceObject(void*){} static void ZwClose(HANDLE){}
static void *ExAllocatePoolWithTag(int,size_t n,int){++allocs;return std::malloc(n);}
static void ExFreePoolWithTag(void *p,int){--allocs;std::free(p);}
static void RtlCopyMemory(void*d,const void*s,size_t n){std::memcpy(d,s,n);}
static NTSTATUS ZwDeviceIoControlFile(HANDLE,void*,void*,void*,IO_STATUS_BLOCK *io,int,void *in,int n,void *out,int){
 ++calls;assert((uintptr_t)out<(uintptr_t)fb || (uintptr_t)out>=(uintptr_t)fb+fbsize);
 if(scenario==1 && calls==2) {io->Information=0;return -1;}
 if(scenario==2) {io->Information=n-4;return 0;}
 std::memcpy(out,in,n);io->Information=n;io->Status=0;return scenario==3?STATUS_PENDING:0;
}
static NTSTATUS KeWaitForSingleObject(void*,int,int,int,void*){return 0;}
static void WRITE_REGISTER_BUFFER_ULONG(volatile ULONG *d,ULONG *s,ULONG n){++writes;for(ULONG i=0;i<n;i++)d[i]=s[i];}
#include "../drivers/gpu/mt8167/wddm-display/ge8300_present.cxx"
int main(){
 alignas(4) static BYTE src[40000],dst[40000];for(size_t i=0;i<sizeof(src);i++)src[i]=(BYTE)i;
 fb=dst;fbsize=sizeof(dst); BLT_INFO a={dst,4000,32,{0,0},0,1000,10};BLT_INFO b={src,4000,32,{0,0},0,1000,10}; RECT r={0,0,1000,10};
 for(scenario=0;scenario<=3;scenario++) {calls=writes=0;std::memset(dst,0,sizeof(dst));
  int ok=A04Ge8300Present(&a,&b,&r);assert(!allocs);
  if(scenario==1){assert(!ok && calls==2 && writes==1);assert(!std::memcmp(dst,src,16384));assert(dst[16384]==0);}
  else if(scenario==2){assert(!ok && calls==1 && writes==0);}
  else {assert(ok && calls==2 && writes==3 && !std::memcmp(dst,src,sizeof(dst)));}
 }
 return 0;
}
