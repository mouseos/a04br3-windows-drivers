/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * New shared-ADC boundaries only. WDF/MMIO mock, not an ADC/hardware result. */
#define A04_SHARED_HP_PCM 1
#include <assert.h>
#include <stdio.h>
#include "hp_parent_stubs/ntddk.h"
#include "../drivers/input/mt8167-accdet/gpio_lease.h"
#include "../drivers/bus/mt8167-shared/thermal_owner.h"
#include "../drivers/bus/mt8167-shared/thermal_pio_record.h"
#include "../drivers/bus/mt8167-shared/codec_headphone.h"
#include "../drivers/bus/mt8167-shared/headphone_interface.h"
struct parent{
 struct mt8167_accdet_gpio_lease accdetGpio;
 volatile LONG admissionClosed,removed,thermalPioUsed,adcPurpose;
 PVOID topck,infra,apmixed,gpio,audioAfe;ULONG apmixedLength;PHYSICAL_ADDRESS thermalAssigned[2];
 struct a04_shared_thermal_owner thermal;PVOID thermalPioMappings[2];ULONGLONG thermalPioDeadline;int thermalRundown;
 int adcProofValid;A04_THERMAL_PIO_RECORD adcProof;
 struct a04_hp_calibration_report hpCali;ULONG hpCaliSaved[4];
 struct a04_hp_route hpRoute;struct a04_hp_measurements hpMeasured;struct a04_hp_calibration hpCalibration;
 ULONG hpPhaseMask[2];int hpCalibrationValid,hpRetainedMute;ULONGLONG hpRouteDeadline;
 struct{int leased,poisoned;}audio;struct{int active,poisoned;}voice;struct{int held,poisoned;}codecDomain;
 struct{int retained,prepared,poisoned;}digitalPlayback;struct{int held,poisoned;}audioTopClock;int afeClockHeld,afeClockPoisoned;
 struct{int online,poisoned;}regs;int classd;
};typedef struct parent A04_PARENT_CONTEXT;
struct request{PVOID buffer;size_t size,information;NTSTATUS status;};
static ULONG th[1024],adc[1024],top[1024],infra[1024],analog[1024],afe[1024],gpio[1024];
static int calibrationMock;
static ULONG proofVersion=2;
static int power,refs,maps,locks,mapCalls,failMap,ready=1,getProof,setProof,sampleTimeout,jackInserted,drained;
static ULONGLONG now;
static ULONG READ_REGISTER_ULONG(volatile ULONG*p){
 if(calibrationMock&&p==afe+0x44/4&&(afe[0x10/4]&2))return afe[0x40/4]+(ULONG)((now/10*1764/10000)%49152)&~3u;
 return *p;
}
static void WRITE_REGISTER_ULONG(volatile ULONG*p,ULONG v){*p=v;}
static USHORT READ_REGISTER_USHORT(volatile USHORT*p){
 uintptr_t off=(uintptr_t)p-(uintptr_t)adc;
 if(off==0x30||off==0x34||off==0x40){unsigned ch=off==0x30?7:off==0x34?8:11;USHORT trigger=*(USHORT*)((UCHAR*)adc+4);return trigger&(1u<<ch)?sampleTimeout?0:(USHORT)(0x1000+2000+(ch-7)*100+(calibrationMock?((analog[0x704/4]&((3u<<11)|(3u<<14)))?((analog[0x708/4]&(1u<<28))?20:10):0):0)):0;}
 return *p;
}
static void WRITE_REGISTER_USHORT(volatile USHORT*p,USHORT v){*p=v;}
static ULONGLONG KeQueryInterruptTime(void){return now;}
static int KeGetCurrentIrql(void){return 0;}
static void KeStallExecutionProcessor(unsigned us){now+=(ULONGLONG)us*10;}
static NTSTATUS KeDelayExecutionThread(int mode,int alert,LARGE_INTEGER*p){(void)mode;(void)alert;now+=(ULONGLONG)-p->QuadPart;return 0;}
static NTSTATUS WdfDeviceStopIdle(WDFDEVICE p,int wait){(void)p;(void)wait;++power;return 0;}
static void WdfDeviceResumeIdle(WDFDEVICE p){(void)p;--power;assert(power>=0);}
static int ExAcquireRundownProtection(int*p){++*p;return 1;}
static void ExReleaseRundownProtection(int*p){--*p;assert(*p>=0);}
static void WdfObjectReference(WDFDEVICE p){(void)p;++refs;}
static void WdfObjectDereference(WDFDEVICE p){(void)p;--refs;assert(refs>=0);}
static PVOID WdfObjectContextGetObject(PVOID p){return p;}
static A04_PARENT_CONTEXT*ParentContext(WDFDEVICE p){return p;}
static int ParentLock(A04_PARENT_CONTEXT*p){if(p->admissionClosed)return -1;assert(!locks);locks=1;return 0;}
static void ParentUnlock(A04_PARENT_CONTEXT*p){(void)p;assert(locks);locks=0;}
static PVOID MmMapIoSpace(PHYSICAL_ADDRESS address,size_t size,int type){(void)type;assert(size==0x1000);if(++mapCalls==failMap)return 0;++maps;return address.QuadPart==1?th:adc;}
static void MmUnmapIoSpace(PVOID p,size_t size){assert((p==th||p==adc)&&size==0x1000);--maps;assert(maps>=0);}
static NTSTATUS ExGetFirmwareEnvironmentVariable(UNICODE_STRING*n,GUID*g,PVOID out,ULONG*bytes,ULONG*attrs){
 (void)n;(void)g;assert(*bytes==72);A04_THERMAL_PIO_RECORD*r=out;memset(r,0,sizeof(*r));r->bytes=72;r->version=proofVersion;r->ready=ready;r->stage=6;r->reset_after=1;r->pause_after=0x10e;*attrs=6;getProof++;return 0;}
static NTSTATUS ExSetFirmwareEnvironmentVariable(UNICODE_STRING*n,GUID*g,PVOID p,ULONG bytes,ULONG attrs){(void)n;(void)g;assert(!p&&!bytes&&!attrs);setProof++;return 0;}
static int a04_mt6392_classd_verify_muted(int*p){return *p;}
static int ParentDomainRead(void*p,unsigned offset,uint32_t*out){A04_PARENT_CONTEXT*c=p;*out=*((ULONG*)((UCHAR*)c->apmixed+offset));return 0;}
static int ParentDomainWrite(void*p,unsigned offset,uint32_t value){A04_PARENT_CONTEXT*c=p;*((ULONG*)((UCHAR*)c->apmixed+offset))=value;return 0;}
static int ParentVerifyDmaDrained(PVOID p){(void)p;return drained?0:-1;}
static int ParentAccdetRead32(void*p,unsigned off,uint32_t*out){A04_PARENT_CONTEXT*c=p;if(off==0x210)*out=jackInserted?0:8;else *out=*((ULONG*)((UCHAR*)c->gpio+off));return 0;}
static int ParentAccdetWrite32(void*p,unsigned off,uint32_t v){A04_PARENT_CONTEXT*c=p;*((ULONG*)((UCHAR*)c->gpio+off))=v;return 0;}
static int ParentAccdetLeaseLive(A04_PARENT_CONTEXT*c){struct mt8167_accdet_gpio_io io={c,ParentAccdetRead32,ParentAccdetWrite32};return c->removed||!c->regs.online||c->regs.poisoned?-1:mt8167_accdet_gpio_live(&c->accdetGpio,&io);}
static WDFDEVICE WdfIoQueueGetDevice(WDFQUEUE q){return q;}
static NTSTATUS WdfRequestRetrieveOutputBuffer(WDFREQUEST r,size_t bytes,PVOID*out,PVOID n){(void)n;if(r->size<bytes)return STATUS_INVALID_PARAMETER;*out=r->buffer;return 0;}
static void WdfRequestComplete(WDFREQUEST r,NTSTATUS s){r->status=s;r->information=0;}
static void WdfRequestCompleteWithInformation(WDFREQUEST r,NTSTATUS s,size_t n){r->status=s;r->information=n;}
#include "../drivers/bus/mt8167-shared/thermal_native_parent.inc"
#include "../drivers/bus/mt8167-shared/headphone_parent.inc"
static struct parent initial(void){struct parent c={0};memset(th,0,sizeof(th));memset(adc,0,sizeof(adc));memset(top,0,sizeof(top));memset(infra,0,sizeof(infra));memset(analog,0,sizeof(analog));memset(afe,0,sizeof(afe));memset(gpio,0,sizeof(gpio));c.topck=top;c.infra=infra;c.apmixed=analog;c.apmixedLength=4096;c.audioAfe=afe;c.gpio=gpio;c.regs.online=1;c.thermal.online=1;c.thermalAssigned[0].QuadPart=1;c.thermalAssigned[1].QuadPart=2;infra[0x128/4]=proofVersion==2?1:0;th[0x3c/4]=proofVersion==3?0x10e:0;mapCalls=0;failMap=0;sampleTimeout=0;now=0;struct mt8167_accdet_gpio_io io={&c,ParentAccdetRead32,ParentAccdetWrite32};assert(!mt8167_accdet_gpio_acquire(&c.accdetGpio,&io));return c;}
int main(int argc,char **argv){
 if(argc>1){assert(argv[1][0]=='3');proofVersion=3;}
 struct parent c=initial();ULONG generation=0,stage=0,adopted=0,raw[3]={0};struct a04_hp_raw_sample sample;
 assert(sizeof(sample)==72&&sizeof(struct a04_hp_raw_pair)==160);
 ready=0;assert(ParentHpAdcBegin(&c,&generation)==STATUS_DEVICE_NOT_READY&&!generation&&!maps&&!refs&&!power&&!c.adcPurpose&&!c.thermalRundown);ready=1;
 for(int fail=1;fail<=2;fail++){c=initial();failMap=fail;assert(!NT_SUCCESS(ParentHpAdcBegin(&c,&generation))&&!maps&&!refs&&!power&&!c.adcPurpose&&!c.thermal.leased&&!c.thermalRundown);}
 c=initial();getProof=setProof=0;assert(ParentHpAdcBegin(&c,&generation)==0&&generation==1&&maps==2&&power==1&&refs==1&&!c.thermalRundown&&c.thermal.leased);
 assert(ParentThermalPioMeasure(&c,&adopted,raw,&stage)==STATUS_DEVICE_BUSY&&!c.thermalPioUsed);assert(ParentHpAdcBegin(&c,&stage)==STATUS_DEVICE_BUSY);
 assert(ParentHpAdcRead(&c,generation+1,7,&sample)==STATUS_DEVICE_NOT_READY&&!sample.valid);
 assert(ParentHpAdcRead(&c,generation,7,&sample)==0&&sample.valid==1&&sample.channel==7&&sample.phase==0&&sample.raw[0]==2000&&!c.hpCalibrationValid);
 assert(ParentHpAdcRead(&c,generation,8,&sample)==0&&sample.raw[21]==2100&&sample.phase==0&&!c.hpCalibrationValid);
 assert(ParentHpAdcEnd(&c,generation+1)==STATUS_DEVICE_NOT_READY&&maps==2&&power==1);
 assert(ParentHpAdcEnd(&c,generation)==0&&!maps&&!power&&!refs&&!c.adcPurpose&&!c.thermal.leased&&!c.thermalRundown);
 assert(ParentThermalPioMeasure(&c,&adopted,raw,&stage)==0&&raw[0]==2400&&raw[1]==2400&&raw[2]==2400&&!maps&&!refs&&!power&&getProof==1&&setProof==1);
 assert(ParentHpAdcBegin(&c,&generation)==0&&generation==2&&getProof==1&&setProof==1);now=c.thermalPioDeadline+1;
 assert(ParentHpAdcRead(&c,generation,7,&sample)==STATUS_DEVICE_NOT_READY&&!sample.valid);assert(ParentHpAdcEnd(&c,generation)==0&&!maps&&!power&&!refs);
 c=initial();assert(ParentHpAdcBegin(&c,&generation)==0);sampleTimeout=1;
 assert(ParentHpAdcRead(&c,generation,7,&sample)==STATUS_DEVICE_HARDWARE_ERROR&&!sample.valid&&c.thermal.poisoned&&c.thermal.leased&&maps==2&&power==1&&refs==1&&!c.thermalRundown);
 assert(ParentHpAdcEnd(&c,generation)==STATUS_DEVICE_NOT_READY&&maps==2&&power==1&&refs==1);
 c.admissionClosed=1;a04_shared_thermal_stop(&c.thermal);
 assert(ParentHpAdcRead(&c,generation,7,&sample)==STATUS_DEVICE_NOT_READY&&!c.thermalRundown&&maps==2&&power==1&&refs==1);
 /* One actual producer invocation, MMIO/ADC/timer model only. */
 maps=power=refs=0;c=initial();c.audio.leased=c.codecDomain.held=c.afeClockHeld=c.audioTopClock.held=1;
 analog[0x704/4]=1u<<13;analog[0x708/4]=(1u<<16)|(1u<<17)|(1u<<27);
 calibrationMock=1;assert(ParentHpCalibratePrepare(&c,0x100000,49152,&generation)==0&&generation&&!c.hpCalibrationValid&&c.hpCali.state==1);
 afe[0x40/4]=0x100000;afe[0x48/4]=0x10bfff;afe[0x10/4]|=2;afe[0x3a0/4]=1;
 assert(ParentHpCalibrateMeasure(&c,generation)==0&&c.hpCali.state==3&&c.hpCali.phase_mask==7&&!c.hpCalibrationValid);
 assert(c.hpCali.uv[0][0]!=c.hpCali.uv[0][1]&&c.hpCali.uv[0][1]!=c.hpCali.uv[0][2]);
 assert(ParentHpCalibrateFinish(&c,generation)==STATUS_DEVICE_HARDWARE_ERROR&&!c.hpCalibrationValid&&c.audio.poisoned&&c.thermal.leased);
 maps=power=refs=0;c=initial();c.audio.leased=c.codecDomain.held=c.afeClockHeld=c.audioTopClock.held=1;
 analog[0x704/4]=1u<<13;analog[0x708/4]=(1u<<16)|(1u<<17)|(1u<<27);
 assert(ParentHpCalibratePrepare(&c,0x100000,49152,&generation)==0);
 afe[0x40/4]=0x100000;afe[0x48/4]=0x10bfff;afe[0x10/4]|=2;afe[0x3a0/4]=1;
 assert(ParentHpCalibrateMeasure(&c,generation)==0);
 afe[0x10/4]&=~2u;afe[0x3a0/4]=0;
 assert(ParentHpCalibrateFinish(&c,generation)==0&&c.hpCalibrationValid&&c.hpCali.state==6&&c.hpCali.same_buffer&&c.hpCali.stop_observed&&!maps&&!power&&!refs);
 assert(c.digitalPlayback.prepared&&c.digitalPlayback.retained&&afe[0x40/4]==0x100000&&afe[0x48/4]==0x10bfff);
 assert((afe[0xf04/4]&15)==8&&((afe[0x34/4]>>8)&15)==10);
 assert(ParentHpCalibratePrepare(&c,0x100000,49152,&stage)==STATUS_DEVICE_BUSY);
 puts("HP producer: same-ring44100 actual A/B/C capture+conversion, stop refusal while DMA runs, retained48000 handoff and no repeat PASS (mock values only)");
 calibrationMock=0;
 /* Fresh simulated boot after intentionally retained cases (no real maps). */
 for(unsigned fault=0;fault<6;fault++){
  maps=power=refs=0;c=initial();assert(ParentHpAdcBegin(&c,&generation)==0);
  switch(fault){case 0:top[0x24/4]=2;break;case 1:if(proofVersion==3)th[0x3c/4]=0;else infra[0x128/4]=0;break;
   case 2:th[0xc/4]=1;break;case 3:th[0x10/4]=1;break;
   case 4:th[0x418/4]=1u<<16;break;case 5:adc[0]=1;break;}
  assert(ParentHpAdcEnd(&c,generation)==STATUS_DEVICE_HARDWARE_ERROR&&c.thermal.poisoned&&maps==2&&power==1&&refs==1);
 }
 maps=power=refs=0;c=initial();
 /* Deliberately synthetic calibration fixture exercises ONLY lifetimes.
  * No production API accepts coefficients or promotes raw to this state. */
 c.audio.leased=c.codecDomain.held=c.hpCalibrationValid=c.digitalPlayback.prepared=c.digitalPlayback.retained=c.afeClockHeld=c.audioTopClock.held=1;
 c.hpCalibration.generation=77;c.hpCalibration.measured_channels=3;c.hpCalibration.cap_index=1;
 analog[0x704/4]=1u<<13;analog[0x708/4]=(1u<<16)|(1u<<17)|(1u<<27);
 ULONG state=99;
 assert(ParentHpStart(&c)==STATUS_DEVICE_NOT_READY&&!c.hpRoute.held);
 jackInserted=1;c.digitalPlayback.prepared=0;assert(ParentHpStart(&c)==STATUS_DEVICE_NOT_READY&&!c.hpRoute.held);
 c.digitalPlayback.prepared=1;assert(ParentHpStart(&c)==0&&c.hpRoute.held&&c.hpRetainedMute);
 assert((analog[0x708/4]&((1u<<25)|(1u<<22)))==((1u<<25)|(1u<<22)));
 assert(ParentHpResume(&c)==0&&!c.hpRetainedMute);
 assert(!(analog[0x708/4]&((1u<<25)|(1u<<22))));
 assert(ParentHpStop(&c,&state)==0&&state==A04_HP_DRAIN_RETAINED&&c.hpRoute.held&&c.hpRoute.active&&c.hpRetainedMute);
 assert(ParentHpResume(&c)==0&&c.hpRoute.held);
 c.admissionClosed=1;assert(ParentHpStop(&c,&state)==STATUS_DEVICE_NOT_READY&&state==A04_HP_DRAIN_UNKNOWN&&c.hpRoute.held);
 c.admissionClosed=0;drained=1;
 assert(ParentHpStop(&c,&state)==0&&state==A04_HP_DRAIN_RELEASED&&!c.hpRoute.held&&!c.hpRoute.active);
 /* Recreate the explicit mock route for each failed STOP observation. */
 for(unsigned fault=0;fault<5;fault++){
  c=initial();c.audio.leased=c.codecDomain.held=c.hpCalibrationValid=c.digitalPlayback.prepared=c.digitalPlayback.retained=c.afeClockHeld=c.audioTopClock.held=1;
  c.hpCalibration.generation=77;c.hpCalibration.measured_channels=3;c.hpCalibration.cap_index=1;
  analog[0x704/4]=1u<<13;analog[0x708/4]=(1u<<16)|(1u<<17)|(1u<<27);jackInserted=1;
  assert(ParentHpStart(&c)==0);assert(ParentHpResume(&c)==0);
  switch(fault){case 0:afe[0x10/4]=2;break;case 1:afe[0x3a0/4]=1;break;case 2:afe[0x3a4/4]=1;break;case 3:afe[0x10/4]=MAXULONG;break;case 4:afe[0x3a4/4]=MAXULONG;break;}
  assert(ParentHpStop(&c,&state)==STATUS_DEVICE_HARDWARE_ERROR&&state==A04_HP_DRAIN_UNKNOWN&&c.hpRetainedMute&&c.hpRoute.held&&c.audio.poisoned);
 }
 puts("HP STOP parent observation: running DL1, enabled IRQ, pending IRQ and inaccessible MMIO clamp/retain without successful STOP PASS (mock only)");
 puts("HP ADC lifetime: V3 rejection, one-owner exclusion, generation/raw-unknown, clean restore, timeout/remove/reset/clock/master retain PASS");
 puts("HP parent: start-muted, resume, clamp-retained, closed-admission no-free, certified full-stop PASS (mock only)");
}
