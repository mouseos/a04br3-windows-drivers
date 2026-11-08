/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Real property/transport headers with a deterministic kernel-DDI fixture.
 * Not a claim of Windows execution, physical insertion or GPIO qualification. */
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cstdio>
#include <initializer_list>
using LONG=int32_t;using ULONG=uint32_t;using NTSTATUS=int32_t;using WCHAR=wchar_t;
using PWSTR=WCHAR *;using PVOID=void *;using PUCHAR=unsigned char *;
using BOOLEAN=bool;using SCHAR=int8_t;
struct GUID {uint32_t a;uint16_t b,c;uint8_t d[8];};
static GUID KSPROPSETID_Jack={1,0,0,{0}},KSPROPTYPESETID_General={2,0,0,{0}};
static bool IsEqualGUIDAligned(const GUID&a,const GUID&b){return !memcmp(&a,&b,sizeof(a));}
#define TRUE true
#define FALSE false
#define NT_SUCCESS(s) ((s)>=0)
#define STATUS_SUCCESS 0
#define STATUS_PENDING 0x103
#define STATUS_TIMEOUT 0x102
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xc0000010)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009a)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011)
#define STATUS_IO_TIMEOUT ((NTSTATUS)0xc00000b5)
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#define STATUS_MORE_PROCESSING_REQUIRED ((NTSTATUS)0xc0000016)
#define FILE_DEVICE_UNKNOWN 0x22
#define METHOD_BUFFERED 0
#define FILE_READ_DATA 1
#define CTL_CODE(d,f,m,a) (((d)<<16)|((a)<<14)|((f)<<2)|(m))
#define IRP_BUFFERED_IO 0x10
#define IRP_MJ_DEVICE_CONTROL 14
#define IO_NO_INCREMENT 0
#define PASSIVE_LEVEL 0
#define KernelMode 0
#define Executive 0
#define NotificationEvent 0
#define NonPagedPool 0
#define KSPROPERTY_TYPE_GET 1u
#define KSPROPERTY_TYPE_SET 2u
#define KSPROPERTY_TYPE_BASICSUPPORT 0x200u
#define KSPROPERTY_JACK_DESCRIPTION 1
#define KSPROPERTY_JACK_DESCRIPTION2 2
#define JACKDESC2_PRESENCE_DETECT_CAPABILITY 1
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RtlCopyMemory(p,s,n) memcpy(p,s,n)
struct KSMULTIPLE_ITEM{ULONG Size,Count;};
struct KSPROPERTY_DESCRIPTION {ULONG AccessFlags,DescriptionSize;struct {GUID Set;ULONG Id,Flags;}PropTypeSet;ULONG MembersListCount,Reserved;};
enum {eConnTypeUnknown,eGeoLocNotApplicable,eGenLocPrimaryBox,ePortConnJack};
struct KSJACK_DESCRIPTION{ULONG ChannelMapping,Color,ConnectionType,GeoLocation,GenLocation,PortConnection;int32_t IsConnected;};
struct KSJACK_DESCRIPTION2{ULONG DeviceStateInfo,JackCapabilities;};
struct PCPROPERTY_ITEM{GUID *Set;ULONG Id;};
struct PCPROPERTY_REQUEST{PCPROPERTY_ITEM *PropertyItem;ULONG Verb,InstanceSize;PVOID Instance;ULONG ValueSize;PVOID Value;};
using PPCPROPERTY_REQUEST=PCPROPERTY_REQUEST *;
struct DEVICE_OBJECT{SCHAR StackSize;};using PDEVICE_OBJECT=DEVICE_OBJECT *;
struct FILE_OBJECT{};using PFILE_OBJECT=FILE_OBJECT *;
struct KEVENT{bool signal;};
struct LARGE_INTEGER{int64_t QuadPart;};
struct IO_STATUS_BLOCK{NTSTATUS Status;uintptr_t Information;};
struct UNICODE_STRING{uint16_t Length;WCHAR *Buffer;};
struct IO_STACK_LOCATION {int MajorFunction;PFILE_OBJECT FileObject;struct {struct {ULONG IoControlCode,InputBufferLength,OutputBufferLength;}DeviceIoControl;}Parameters;};
using PIO_STACK_LOCATION=IO_STACK_LOCATION *;
struct IRP;using PIRP=IRP *;using COMPLETE=NTSTATUS(*)(PDEVICE_OBJECT,PIRP,PVOID);
struct IRP{int RequestorMode,Flags;struct {PVOID SystemBuffer;}AssociatedIrp;IO_STATUS_BLOCK IoStatus;IO_STACK_LOCATION stack;COMPLETE complete;PVOID context;};
static int pool=0,files=0,irps=0,ddilevel=0,calls=0,cancels=0,opens=0;
static int failure=0,dispatch=0;static PIRP pending;static DEVICE_OBJECT target={2},owner={2};static FILE_OBJECT file;
static NTSTATUS outputStatus=STATUS_SUCCESS;static uintptr_t outputBytes=16;static ULONG inserted=0;
static LONG InterlockedDecrement(volatile LONG*p){return --*p;}
static LONG InterlockedExchange(volatile LONG*p,LONG v){LONG o=*p;*p=v;return o;}
static LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG c){LONG o=*p;if(o==c)*p=v;return o;}
static PVOID ExAllocatePoolWithTag(int,size_t n,int){if(failure==3)return nullptr;++pool;return calloc(1,n);}
static void ExFreePoolWithTag(PVOID p,int){--pool;free(p);}
static void ExFreePool(PVOID p){free(p);}
static void ObDereferenceObject(PVOID){--files;}
static void ObDereferenceObjectDeferDelete(PVOID){--files;}
static int KeGetCurrentIrql(){return ddilevel;}
static NTSTATUS IoGetDeviceInterfaces(const GUID*,PDEVICE_OBJECT,ULONG,PWSTR*p){
 if(failure==1)return STATUS_DEVICE_NOT_READY;
 const WCHAR a[]=L"parent\0\0",b[]=L"parent\0other\0\0",e[]=L"\0";
 const WCHAR*s=failure==7?b:failure==8?e:a;size_t bytes=failure==7?sizeof(b):failure==8?sizeof(e):sizeof(a);
 *p=(PWSTR)malloc(bytes);memcpy(*p,s,bytes);return STATUS_SUCCESS;
}
static void RtlInitUnicodeString(UNICODE_STRING*p,PWSTR s){p->Length=wcslen(s)*sizeof(WCHAR);p->Buffer=s;}
static NTSTATUS IoGetDeviceObjectPointer(UNICODE_STRING*,ULONG access,PFILE_OBJECT*f,PDEVICE_OBJECT*d){
 assert(access==FILE_READ_DATA);++opens;if(failure==2)return STATUS_DEVICE_NOT_READY;++files;*f=&file;*d=&target;return 0;
}
static void KeInitializeEvent(KEVENT*e,int,bool b){e->signal=b;}
static PIRP IoAllocateIrp(SCHAR,bool){if(failure==4)return nullptr;++irps;PIRP p=new IRP{};memset(&p->stack,0xa5,sizeof(p->stack));return p;}
static void IoFreeIrp(PIRP p){--irps;delete p;}
static PIO_STACK_LOCATION IoGetNextIrpStackLocation(PIRP p){return &p->stack;}
static NTSTATUS IoSetCompletionRoutineEx(PDEVICE_OBJECT d,PIRP p,COMPLETE fn,PVOID c,bool,bool,bool){assert(d==&owner);if(failure==5)return STATUS_INSUFFICIENT_RESOURCES;p->complete=fn;p->context=c;return 0;}
static void KeSetEvent(KEVENT*e,int,bool){e->signal=true;}
#include "../drivers/audio/mt8167-afe/jack_property.h"
static void finish(){PIRP p=pending;assert(p);pending=nullptr;p->IoStatus={outputStatus,outputBytes};
 auto*s=(mt8167_accdet_socket*)p->AssociatedIrp.SystemBuffer;*s={16,1,1,inserted};
 assert(p->complete(&target,p,p->context)==STATUS_MORE_PROCESSING_REQUIRED);
}
static NTSTATUS IoCallDriver(PDEVICE_OBJECT d,PIRP p){assert(d==&target);assert(p->stack.MajorFunction==14);assert(p->stack.Parameters.DeviceIoControl.InputBufferLength==0);assert(p->stack.Parameters.DeviceIoControl.OutputBufferLength==16);assert(p->stack.Parameters.DeviceIoControl.IoControlCode==0x226554);++calls;pending=p;if(dispatch==0)finish();return STATUS_PENDING;}
static NTSTATUS KeWaitForSingleObject(KEVENT*e,int,int,bool,LARGE_INTEGER*t){assert(t->QuadPart==-1000000);if(dispatch==1)finish();return e->signal?STATUS_SUCCESS:STATUS_TIMEOUT;}
static bool IoCancelIrp(PIRP){++cancels;if(dispatch==3&&pending)finish();return true;}
#include "../drivers/audio/mt8167-afe/jack_sense.h"
static int reads=0;static NTSTATUS senseStatus=0;static mt8167_accdet_socket sense={16,1,1,0};
static NTSTATUS readSense(PDEVICE_OBJECT,mt8167_accdet_socket*s){++reads;*s=sense;return senseStatus;}
static void balanced(){assert(!pool&&!files&&!irps&&!pending&&!JackRequestActive);}
int main(){
 alignas(16) unsigned char buf[128];ULONG pin=2;PCPROPERTY_ITEM item={&KSPROPSETID_Jack,1};
 PCPROPERTY_REQUEST r={&item,1,sizeof(pin),&pin,sizeof(buf),buf};
 auto prop=[&](ULONG capacity){r.ValueSize=capacity;return Mt8167JackPropertyRead(&r,&owner,readSense);};
 assert(prop(0)==STATUS_BUFFER_OVERFLOW&&r.ValueSize==36&&!reads);
 assert(prop(35)==STATUS_BUFFER_TOO_SMALL&&!reads);
 for(ULONG value=0;value<2;++value){sense.inserted=value;memset(buf,0xa5,sizeof(buf));assert(prop(sizeof(buf))==0);auto*m=(KSMULTIPLE_ITEM*)buf;auto*d=(KSJACK_DESCRIPTION*)(m+1);assert(m->Count==1&&m->Size==36&&r.ValueSize==36&&d->IsConnected==(int)value);assert(!d->ChannelMapping&&!d->Color&&buf[36]==0xa5);}
 item.Id=2;assert(prop(0)==STATUS_BUFFER_OVERFLOW&&r.ValueSize==16);assert(prop(sizeof(buf))==0);auto*d2=(KSJACK_DESCRIPTION2*)(buf+8);assert(d2->JackCapabilities==1&&!d2->DeviceStateInfo);
 for(NTSTATUS s:{STATUS_DEVICE_NOT_READY,STATUS_DEVICE_BUSY,STATUS_IO_TIMEOUT}){senseStatus=s;memset(buf,0xa5,sizeof(buf));assert(prop(sizeof(buf))==s&&r.ValueSize==0&&buf[0]==0xa5);}senseStatus=0;
 for(int field=0;field<4;++field){auto saved=sense;reinterpret_cast<ULONG*>(&sense)[field]=field==3?2:0;assert(prop(sizeof(buf))==STATUS_DEVICE_NOT_READY&&r.ValueSize==0);sense=saved;}
 for(pin=0;pin<2;++pin){int before=reads;assert(prop(sizeof(buf))==0&&r.ValueSize==8&&((KSMULTIPLE_ITEM*)buf)->Count==0&&reads==before);}
 pin=3;assert(prop(sizeof(buf))==STATUS_INVALID_PARAMETER);pin=2;r.InstanceSize=0;assert(prop(sizeof(buf))==STATUS_INVALID_PARAMETER);r.InstanceSize=4;
 r.Verb=KSPROPERTY_TYPE_SET;assert(prop(sizeof(buf))==STATUS_INVALID_DEVICE_REQUEST);r.Verb=KSPROPERTY_TYPE_BASICSUPPORT;int before=reads;assert(prop(4)==0&&*(ULONG*)buf==(1|0x200)&&reads==before);assert(prop(sizeof(buf))==0&&r.ValueSize==sizeof(KSPROPERTY_DESCRIPTION));r.Verb=1;
 mt8167_accdet_socket sample;
 for(dispatch=0;dispatch<=1;++dispatch){assert(Mt8167ReadParentJack(&owner,&sample)==0&&sample.qualified==1);balanced();}
 for(failure=1;failure<=8;++failure){if(failure==6)continue;assert(!NT_SUCCESS(Mt8167ReadParentJack(&owner,&sample)));balanced();}failure=0;
 outputBytes=15;dispatch=0;assert(Mt8167ReadParentJack(&owner,&sample)==STATUS_DEVICE_NOT_READY);balanced();outputBytes=16;
 outputStatus=STATUS_DEVICE_NOT_READY;assert(Mt8167ReadParentJack(&owner,&sample)==outputStatus);balanced();outputStatus=0;
 /* Timeout: no caller-stack/output reference survives, late completion owns resources.
  * A second caller cannot accumulate requests while the first is pending. */
 dispatch=2;assert(Mt8167ReadParentJack(&owner,&sample)==STATUS_IO_TIMEOUT);assert(pool==1&&files==1&&irps==1&&pending&&JackRequestActive);before=calls;assert(Mt8167ReadParentJack(&owner,&sample)==STATUS_DEVICE_BUSY&&calls==before);finish();balanced();
 dispatch=3;assert(Mt8167ReadParentJack(&owner,&sample)==STATUS_IO_TIMEOUT);balanced();
 ddilevel=2;before=opens;assert(Mt8167ReadParentJack(&owner,&sample)==STATUS_INVALID_DEVICE_STATE&&opens==before);ddilevel=0;
 dispatch=0;item.Id=1;pin=2;r.ValueSize=sizeof(buf);inserted=1;assert(Mt8167JackPropertyRead(&r,&owner,Mt8167ReadParentJack)==0&&((KSJACK_DESCRIPTION*)(buf+8))->IsConnected);balanced();
 puts("AFE jack: property ABI/read failures/timeout+late completion/admission/resource balance PASS");
}
