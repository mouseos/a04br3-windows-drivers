/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Executes production controller.c with host WDF/owner mocks. Exercises only
 * publication ordering/ref rundown; does not model SDIO/RF or prove device start.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define A04_MT6630_COMBO_CONTROLLER_H
#define A04_MT6630_BROKER_ABI_H
#define A04_BT_HFP_TARGET_H
typedef uintptr_t ULONG_PTR;typedef int32_t NTSTATUS; typedef uint32_t ULONG;typedef ULONG *PULONG;
typedef long LONG;typedef void *PVOID;typedef void *PDEVICE_OBJECT;
typedef void *WDFDEVICE;typedef void *WDFIOTARGET;typedef void *WDFWAITLOCK;
typedef int BOOLEAN;typedef int EX_RUNDOWN_REF;
#define TRUE 1
#define FALSE 0
#define PASSIVE_LEVEL 0
#define MAXLONG LONG_MAX
#define STATUS_SUCCESS 0
#define STATUS_DEVICE_NOT_READY ((int32_t)0xc00000a3)
#define STATUS_INVALID_PARAMETER ((int32_t)0xc000000d)
#define STATUS_DEVICE_BUSY ((int32_t)0x80000011)
#define STATUS_INVALID_DEVICE_STATE ((int32_t)0xc0000184)
#define STATUS_DATA_ERROR ((int32_t)0xc000003e)
#define STATUS_PLUGPLAY_NO_DEVICE ((int32_t)0xc000025e)
#define A04_BT_PHYSICAL_OWNER_VERSION 1
#define A04_SHARED_HFP_VERSION 3
#define A04_BT_BROKER_GET_PHYSICAL_OWNER 1
#define A04_BT_BROKER_SCO_HOLD 2
#define A04_BT_BROKER_SCO_RELEASE 3
#define WDF_NO_HANDLE NULL
#define NT_SUCCESS(x) ((NTSTATUS)(x)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
typedef struct {unsigned Size,Version;void *Context;void (*InterfaceReference)(void*);void (*InterfaceDereference)(void*);} INTERFACE,*PINTERFACE;
typedef struct {void *Buffer;} UNICODE_STRING;
typedef struct {void *buffer;} WDF_MEMORY_DESCRIPTOR;
typedef int WDF_QUERY_INTERFACE_CONFIG;
#define WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(p,b,n) ((p)->buffer=(b))
#define WDF_QUERY_INTERFACE_CONFIG_INIT(q,i,g,c) (*(q)=0)
static const int A04Mt6630ControllerReadyGuid=1,A04Mt6630ControllerInterfaceGuid=2;
struct a04_bt_physical_owner {INTERFACE Header;ULONG generation;int (*Live)(void*,ULONG);};
struct a04_bt_broker_state {ULONG version,ready,pcm_command_applied,generation;};
struct A04_HFP_CONTROLLER {unsigned size,version;void *context,*controller;
 void (*Reference)(void*),(*Dereference)(void*);NTSTATUS (*Acquire)(void*,ULONG*);
 BOOLEAN (*Live)(void*,ULONG);NTSTATUS (*Release)(void*,ULONG),(*MatchHfpTarget)(void*,void*);};
struct a04_mt6630_controller_interface {INTERFACE Header;NTSTATUS (*GetController)(void*,struct A04_HFP_CONTROLLER*);};
struct a04_combo_controller {WDFDEVICE root,radio;WDFIOTARGET target;WDFWAITLOCK serial;struct a04_bt_physical_owner physical;UNICODE_STRING ready_link;EX_RUNDOWN_REF live_calls;volatile LONG running,removed,uncertain,holds;BOOLEAN live_closed,ready;};
NTSTATUS a04_combo_controller_stop_locked(struct a04_combo_controller*);
static unsigned registered,enabled,disabled,owner_refs,queries,waited;static NTSTATUS owner_status,register_status,enable_status;
static void owner_ref(void *p){(void)p;owner_refs++;}static void owner_unref(void *p){(void)p;assert(owner_refs);owner_refs--;}
static int owner_live(void *p,ULONG g){(void)p;return g==7;}
static int KeGetCurrentIrql(void){return 0;}
static int ExAcquireRundownProtection(EX_RUNDOWN_REF *p){return !*p;}
static void ExReleaseRundownProtection(EX_RUNDOWN_REF *p){(void)p;}
static void ExInitializeRundownProtection(EX_RUNDOWN_REF *p){*p=0;}
static void ExReInitializeRundownProtection(EX_RUNDOWN_REF *p){*p=0;}
static void ExWaitForRundownProtectionRelease(EX_RUNDOWN_REF *p){*p=1;waited++;}
static void WdfObjectReference(void *p){(void)p;}static void WdfObjectDereference(void *p){(void)p;}
static void WdfWaitLockAcquire(void *p,void *t){(void)p;(void)t;}static void WdfWaitLockRelease(void *p){(void)p;}
static NTSTATUS WdfDeviceStopIdle(void *p,int t){(void)p;(void)t;return 0;}static void WdfDeviceResumeIdle(void *p){(void)p;}
static LONG InterlockedIncrement(volatile LONG*p){return ++*p;}static LONG InterlockedDecrement(volatile LONG*p){return --*p;}static LONG InterlockedExchange(volatile LONG*p,LONG v){LONG x=*p;*p=v;return x;}
static void *WdfDeviceWdmGetDeviceObject(void*p){return p;}static void *WdfDeviceWdmGetPhysicalDevice(void*p){return p;}
static NTSTATUS IoRegisterDeviceInterface(void *p,const void*g,void*n,UNICODE_STRING*out){(void)p;(void)g;(void)n;assert(queries&&owner_refs==1);registered++;if(NT_SUCCESS(register_status))out->Buffer=(void*)1;return register_status;}
static NTSTATUS IoSetDeviceInterfaceState(UNICODE_STRING *p,int enable){assert(p->Buffer&&owner_refs==1);if(enable){enabled++;return enable_status;}disabled++;return 0;}
static void RtlFreeUnicodeString(UNICODE_STRING*p){p->Buffer=NULL;}
static NTSTATUS WdfDeviceAddQueryInterface(void*d,WDF_QUERY_INTERFACE_CONFIG*q){(void)d;(void)q;return 0;}
static NTSTATUS a04_bt_hfp_match_radio(void*a,void*b){(void)a;(void)b;return 0;}
static NTSTATUS WdfIoTargetSendIoctlSynchronously(void*t,void*h,ULONG code,WDF_MEMORY_DESCRIPTOR*in,WDF_MEMORY_DESCRIPTOR*out,void*opts,uintptr_t*used){
 (void)t;(void)h;(void)in;(void)opts;assert(code==A04_BT_BROKER_GET_PHYSICAL_OWNER);queries++;
 if(!NT_SUCCESS(owner_status))return owner_status;
 struct a04_bt_physical_owner *p=out->buffer;p->Header=(INTERFACE){sizeof(*p),1,(void*)1,owner_ref,owner_unref};p->generation=7;p->Live=owner_live;owner_ref(p->Header.Context);*used=sizeof(*p);return 0;
}
#include "../drivers/bluetooth/mt6630/combo_controller.c"
static void reset(void){registered=enabled=disabled=owner_refs=queries=waited=0;owner_status=register_status=enable_status=0;}
int main(void){struct a04_combo_controller c;
 reset();register_status=STATUS_PLUGPLAY_NO_DEVICE;
 assert(!a04_combo_controller_init(&c,(void*)1,(void*)2,(void*)3,(void*)4));assert(!registered&&!queries&&!c.running);
 assert(a04_combo_controller_start_locked(&c)==STATUS_PLUGPLAY_NO_DEVICE);assert(registered==1&&!enabled&&!owner_refs&&!c.running&&!c.ready);
 register_status=0;assert(!a04_combo_controller_start_locked(&c));assert(registered==2&&enabled==1&&owner_refs==1&&c.running&&c.ready);
 c.holds=1;assert(a04_combo_controller_stop_locked(&c)==STATUS_DEVICE_BUSY);assert(owner_refs==1&&!disabled);
 c.holds=0;assert(!a04_combo_controller_stop_locked(&c));assert(disabled==1&&!owner_refs&&!c.running);a04_combo_controller_destroy(&c);
 reset();assert(!a04_combo_controller_init(&c,(void*)1,(void*)2,(void*)3,(void*)4));owner_status=STATUS_DEVICE_NOT_READY;
 assert(a04_combo_controller_start_locked(&c)==STATUS_DEVICE_NOT_READY);assert(!registered&&!enabled&&!owner_refs);
 owner_status=0;enable_status=STATUS_PLUGPLAY_NO_DEVICE;assert(a04_combo_controller_start_locked(&c)==STATUS_PLUGPLAY_NO_DEVICE);assert(!owner_refs&&!c.running&&!c.ready);a04_combo_controller_destroy(&c);
 puts("production controller publication ordering/failure/retry/held-stop: PASS (host mocks; not RF)");return 0;}
