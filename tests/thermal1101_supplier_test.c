#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
typedef uint64_t ULONGLONG;typedef uint16_t USHORT;typedef uint8_t UCHAR;typedef void *PVOID;typedef uint32_t ULONG;typedef int32_t LONG,NTSTATUS;typedef void *WDFDEVICE;
typedef struct {uint32_t a;uint16_t b,c;uint8_t d[8];} GUID;
typedef struct {int unused;} UNICODE_STRING;
#define RTL_CONSTANT_STRING(x) {0}
#define PASSIVE_LEVEL 0
#define TRUE 1
#define MAXULONG UINT32_MAX
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_DEVICE_NOT_READY (-2)
#define STATUS_DEVICE_HARDWARE_ERROR (-3)
#define STATUS_IO_TIMEOUT (-4)
#define NT_SUCCESS(x) ((x)>=0)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define DPFLTR_IHVDRIVER_ID 0
#define DPFLTR_ERROR_LEVEL 0
#define DbgPrintEx(...) ((void)0)
struct thermal {int leased,native_claim,clocks_ready,poisoned,quiescent,firmware_released,adc_exclusive;};
typedef struct {PVOID topck,infra,apmixed;ULONG apmixedLength;volatile LONG admissionClosed,removed,thermalPioUsed;struct thermal thermal;PVOID thermalPioMappings[2];ULONGLONG thermalPioDeadline;int thermalRundown;} A04_PARENT_CONTEXT;
static A04_PARENT_CONTEXT *ParentContext(WDFDEVICE d){return d;}
static unsigned scenario,idles,refs,rundowns,locked,unmaps,var_present=1,fw_reads,fw_writes,infra_reads,mmio_reads,stalls;
static uint32_t analog=0x330;static uint16_t trigger;
static unsigned char top[4096],infra[4096],apmixed[4096],thermal_mem[4096],adc[4096];
static LONG InterlockedCompareExchange(volatile LONG *p,LONG x,LONG cmp){LONG old=*p;if(old==cmp)*p=x;return old;}
static ULONGLONG KeQueryInterruptTime(void){return scenario==7&&stalls?6000000ULL:0;}
static int KeGetCurrentIrql(void){return 0;}
static NTSTATUS WdfDeviceStopIdle(WDFDEVICE d,int wait){(void)d;(void)wait;idles++;return 0;}
static void WdfDeviceResumeIdle(WDFDEVICE d){(void)d;assert(idles);idles--;}
static int ExAcquireRundownProtection(int *p){(void)p;rundowns++;return 1;}
static void ExReleaseRundownProtection(int *p){(void)p;assert(rundowns);rundowns--;}
static int ParentLock(void *p){A04_PARENT_CONTEXT *c=p;if(c->admissionClosed)return -1;assert(!locked);locked=1;return 0;}
static void ParentUnlock(void *p){(void)p;assert(locked);locked=0;}
static void WdfObjectReference(WDFDEVICE d){(void)d;refs++;}
static void WdfObjectDereference(WDFDEVICE d){(void)d;assert(refs);refs--;}
static void MmUnmapIoSpace(void *p,unsigned n){assert(n==4096&&(p==thermal_mem||p==adc));unmaps++;}
static ULONG READ_REGISTER_ULONG(volatile ULONG *p){uintptr_t a=(uintptr_t)p;assert(locked);mmio_reads++;
 if(a==(uintptr_t)top+0x24)return 0;
 if(a==(uintptr_t)infra+0x128){infra_reads++;return scenario==4&&infra_reads==2?MAXULONG:1;}
 if(a==(uintptr_t)apmixed+0x604)return scenario==5?MAXULONG:analog;
 if(a==(uintptr_t)thermal_mem||a==(uintptr_t)thermal_mem+0xc||a==(uintptr_t)thermal_mem+0x10||a==(uintptr_t)thermal_mem+0x418)return 0;
 assert(0);return 0;}
static void WRITE_REGISTER_ULONG(volatile ULONG *p,ULONG v){assert(locked&&(uintptr_t)p==(uintptr_t)apmixed+0x604);analog=v;}
static uint16_t READ_REGISTER_USHORT(volatile uint16_t *p){uintptr_t a=(uintptr_t)p;assert(locked);mmio_reads++;
 if(a==(uintptr_t)adc)return 0;if(a==(uintptr_t)adc+4)return trigger;if(a==(uintptr_t)adc+0x10)return 0;
 if(a==(uintptr_t)adc+0x40)return trigger?(scenario==3?0:(uint16_t)(0x1a10+(analog&3))):0;
 assert(0);return 0;}
static void WRITE_REGISTER_USHORT(volatile uint16_t *p,uint16_t v){assert(locked&&(uintptr_t)p==(uintptr_t)adc+4);if(scenario==6&&trigger&&v==0)return;trigger=v;}
static void KeStallExecutionProcessor(unsigned us){assert(us==25||us==200||us==1000);stalls++;}
#include "thermal_pio_record.h"
static NTSTATUS ExGetFirmwareEnvironmentVariable(UNICODE_STRING *n,GUID *g,void *p,ULONG *bytes,ULONG *attrs){A04_THERMAL_PIO_RECORD r={0};(void)n;(void)g;assert(!locked);fw_reads++;
 if(!var_present)return STATUS_DEVICE_NOT_READY;r.version=2;r.bytes=72;r.ready=1;r.stage=6;r.reset_after=1;r.pause_after=0x10e;
 memcpy(p,&r,72);*bytes=72;*attrs=scenario==1?7:6;return 0;}
static NTSTATUS ExSetFirmwareEnvironmentVariable(UNICODE_STRING *n,GUID *g,void *p,ULONG bytes,ULONG attrs){(void)n;(void)g;assert(!locked&&!p&&!bytes&&!attrs);fw_writes++;if(scenario==2)return -1;var_present=0;return 0;}
#include "thermal_native_parent.inc"
int main(int argc,char **argv){A04_PARENT_CONTEXT c={0};ULONG adopted=99,stage=0,raw[3]={0xbeef,0xbeef,0xbeef};NTSTATUS status;assert(argc==2);scenario=(unsigned)atoi(argv[1]);
 c.topck=top;c.infra=infra;c.apmixed=apmixed;c.apmixedLength=4096;
 status=ParentThermalPioMeasure(&c,thermal_mem,adc,&adopted,raw,&stage);
 assert(!locked&&!rundowns);
 if(scenario==0){assert(status==0&&adopted==1&&raw[0]==0xa10&&raw[1]==0xa11&&raw[2]==0xa12&&unmaps==2&&!refs&&!idles&&analog==0x330&&!trigger&&stage==10);}
 else if(scenario==1||scenario==2){assert(status<0&&!adopted&&!unmaps&&!refs&&!idles&&!mmio_reads&&raw[0]==0xbeef);}
 else if(scenario==3){assert(status==STATUS_IO_TIMEOUT&&adopted==1&&unmaps==2&&!refs&&!idles&&!trigger&&raw[0]==0xbeef);}
 else {assert(status==STATUS_DEVICE_HARDWARE_ERROR&&adopted==1&&!unmaps&&refs==1&&idles==1&&c.thermal.poisoned&&c.thermal.leased&&raw[0]==0xbeef);}
 {unsigned reads=mmio_reads,writes=fw_writes;assert(ParentThermalPioMeasure(&c,thermal_mem,adc,&adopted,raw,&stage)==STATUS_DEVICE_NOT_READY);assert(mmio_reads==reads&&fw_writes==writes);}
 assert(stalls<=100);return 0;}
