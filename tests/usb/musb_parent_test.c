/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <ntddk.h>
#include <wdf.h>
#include "../../drivers/bus/mt8167-shared/usb_owner.h"
#include "../../drivers/bus/mt8167-shared/musb_parent_state.h"
#include "../../drivers/usb/mt8167-usb20/board_pins.h"
void *memset(void *p,int v,SIZE_T n){volatile UCHAR*d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(UCHAR)v;return p;}
void *memcpy(void *p,const void*s,SIZE_T n){volatile UCHAR*d=p;const volatile UCHAR*a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
typedef struct {struct mt_musb_parent musb;struct a04_usb_owner usb;PVOID gpio;ULONG removed;FAST_MUTEX pwrapLock;} A04_PARENT_CONTEXT;
#define ParentContext(p) ((A04_PARENT_CONTEXT *)(p))
static UCHAR macmem[0x10000],phymem[0x10000];static ULONG gp[1024],csr[16][2],top[64],pmic,pmicstage,depth,writes;
static ULONGLONG now=10000000;static ULONG stuck,voltage=1,external_voltage,oc_trip,off_stuck,verify_fail;
static int ParentLock(void *p){(void)p;if(depth)return -1;depth=1;return 0;}
static void ParentUnlock(void *p){(void)p;depth=0;}
#define KeGetCurrentIrql() PASSIVE_LEVEL
#define ExAcquireFastMutex(p) ((void)(p),depth=1)
#define ExReleaseFastMutex(p) ((void)(p),depth=0)
#define KeQueryInterruptTime() now
static VOID Delay(KPROCESSOR_MODE m,BOOLEAN a,PLARGE_INTEGER t){(void)m;(void)a;now-=(ULONGLONG)t->QuadPart;if(oc_trip)gp[0x210/4]&=~(1u<<5);}
#define KeDelayExecutionThread Delay
static UCHAR Read8(volatile UCHAR *p){return *p;}
static USHORT Read16(volatile USHORT *p){ULONG_PTR o=(ULONG_PTR)p-(ULONG_PTR)macmem;if(o==0x12||o==0x16)return (USHORT)csr[macmem[0xe]][o==0x16];return *p;}
static ULONG Read32(volatile ULONG *p){return *p;}
static VOID Write8(volatile UCHAR *p,UCHAR v){writes++;if(p==macmem+0xa){*p&=~v;return;}*p=v;}
static VOID Write16(volatile USHORT *p,USHORT v){ULONG_PTR o=(ULONG_PTR)p-(ULONG_PTR)macmem;writes++;if(o==0x12||o==0x16){csr[macmem[0xe]][o==0x16]=stuck?1:0;return;}if(o==2||o==4){*p&=~v;return;}*p=v;}
static VOID Write32(volatile ULONG *p,ULONG v){writes++;*p=v;}
#define READ_REGISTER_UCHAR Read8
#define READ_REGISTER_USHORT Read16
#define READ_REGISTER_ULONG Read32
#define WRITE_REGISTER_UCHAR Write8
#define WRITE_REGISTER_USHORT Write16
#define WRITE_REGISTER_ULONG Write32
static PVOID Map(PHYSICAL_ADDRESS a,SIZE_T n,MEMORY_CACHING_TYPE t){(void)n;(void)t;return a.QuadPart==0x21100000?macmem:phymem;}
#define MmMapIoSpace Map
#define MmUnmapIoSpace(p,n) ((void)(p),(void)(n))
static unsigned ParentUsbGpioRead(void*p,unsigned o){(void)p;return gp[o/4];}
static void ParentUsbGpioWrite(void*p,unsigned o,unsigned v){unsigned k;(void)p;writes++;k=o/4;if((o&15)==4)gp[k-1]|=v;else if((o&15)==8){if(!(o==0x118&&off_stuck))gp[k-2]&=~v;}else gp[k]=v;
 if(o==0x114||o==0x118){if((gp[0x110/4]&(1u<<6))&&voltage){macmem[0x60]=(UCHAR)((macmem[0x60]&~0x18)|0x18);gp[0x220/4]&=~4u;}
 else if(!external_voltage){macmem[0x60]&=~0x18u;gp[0x220/4]|=4u;}}}
#include "../../drivers/bus/mt8167-shared/musb_parent.inc"
static int Verify(void*p){A04_PARENT_CONTEXT*c=p;return verify_fail?-1:A04UsbPinsPrepare(&c->usb.gpio);}
static int Oc(void*p){(void)p;return (gp[0x210/4]&(1u<<5))?0:-1;}
static int Source(void*p){(void)p;return 0;}
static unsigned TopRead(void*p,unsigned o){(void)p;return top[o/4];}
static void TopWrite(void*p,unsigned o,unsigned v){(void)p;if(o==0x84)top[0x24/4]&=~v;if(o==0x9c)top[0x3c/4]&=~v;}
static uint32_t PmicRead(void*p,unsigned o){(void)p;if(o==0x9c)return 1;return pmicstage?(6u<<16)|pmic:0;}
static void PmicWrite(void*p,unsigned o,uint32_t v){(void)p;if(o==0xa8){pmicstage=0;return;}if(v>>31)pmic=v&0xffff;else pmicstage=1;}
static void PmicDelay(void*p){(void)p;}
static void Setup(A04_PARENT_CONTEXT*c,struct a04_pmic_read *bus){RtlZeroMemory(c,sizeof(*c));RtlZeroMemory(gp,sizeof(gp));RtlZeroMemory(macmem,sizeof(macmem));RtlZeroMemory(phymem,sizeof(phymem));RtlZeroMemory(csr,sizeof(csr));depth=stuck=external_voltage=oc_trip=off_stuck=verify_fail=0;voltage=1;
 c->gpio=gp;c->musb.bound=c->musb.accessible=1;c->musb.generation=7;c->musb.mac_pa.QuadPart=0x21100000;c->musb.phy_pa.QuadPart=0x21110000;
 c->usb.online=1;c->usb.context=c;c->usb.lock=ParentLock;c->usb.unlock=ParentUnlock;c->usb.verify_upstream=Verify;c->usb.verify_overcurrent=Oc;c->usb.otg_acquire=Source;c->usb.otg_release=Source;c->usb.top_read=TopRead;c->usb.top_write=TopWrite;c->usb.gpio.context=c;c->usb.gpio.read32=ParentUsbGpioRead;c->usb.gpio.write32=ParentUsbGpioWrite;
 bus->context=c;bus->read32=PmicRead;bus->write32=PmicWrite;bus->delay=PmicDelay;bus->attempts=4;bus->poisoned=0;c->usb.pmic=bus;
 gp[0x210/4]=1u<<5;gp[0x220/4]=1u<<2;macmem[0x6c]=0x34;macmem[0x6d]=0x12;pmic=0x11;pmicstage=0;}
int main(void){A04_PARENT_CONTEXT c;struct a04_pmic_read bus;MT_MUSB_OBSERVATION o;ULONG old,n;
 Setup(&c,&bus);gp[0x220/4]=1u<<9;old=writes;
 CHECK(!NT_SUCCESS(ParentMusbQuery((PVOID)&c,&o))&&o.reason==MT_MUSB_WAIT_ROLE&&writes==old);
 CHECK(!NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7))&&!c.usb.leased&&writes==old);
 gp[0x220/4]=1u<<2;CHECK(NT_SUCCESS(ParentMusbQuery((PVOID)&c,&o))&&(o.evidence&MT_MUSB_QUERY_READY));
 CHECK(!NT_SUCCESS(ParentMusbAcquire((PVOID)&c,6))&&!c.usb.leased);
 if(!NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7)))return 100+c.musb.stage;CHECK(c.musb.held&&c.usb.leased);
 CHECK(NT_SUCCESS(ParentMusbObserve((PVOID)&c,&o))&&o.oc_clear&&(o.valid&MT_MUSB_STOP_PROOF)==MT_MUSB_STOP_PROOF);
 phymem[0x800+0x6d]=0x3e;old=writes;
 CHECK(!NT_SUCCESS(ParentMusbVbus((PVOID)&c,TRUE))&&writes==old&&!c.musb.powered);
 CHECK(NT_SUCCESS(ParentMusbObserve((PVOID)&c,&o))&&!(o.valid&MT_MUSB_VBUS));phymem[0x800+0x6d]=0;
 CHECK(NT_SUCCESS(ParentMusbVbus((PVOID)&c,TRUE))&&c.musb.powered);
 CHECK(NT_SUCCESS(ParentMusbObserve((PVOID)&c,&o))&&o.vbus_on&&(o.evidence&MT_MUSB_NO_FORCED_VBUS));
 CHECK(NT_SUCCESS(ParentMusbVbus((PVOID)&c,TRUE))); /* idempotent source */
 gp[0x210/4]=0;CHECK(NT_SUCCESS(ParentMusbObserve((PVOID)&c,&o))&&!o.oc_clear&&!(gp[0x110/4]&(1u<<6))&&c.musb.poisoned);gp[0x210/4]=1u<<5;
 for(n=0;n<16;n++)csr[n][0]=1;
 CHECK(NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o))&&o.source_off&&!o.vbus_on&&(o.valid&MT_MUSB_STOP_PROOF)==MT_MUSB_STOP_PROOF);
 CHECK(NT_SUCCESS(ParentMusbRelease((PVOID)&c))&&!c.musb.held&&!c.usb.leased);
 Setup(&c,&bus);CHECK(NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7)));voltage=0;
 CHECK(!NT_SUCCESS(ParentMusbVbus((PVOID)&c,TRUE))&&!(gp[0x110/4]&(1u<<6))&&c.musb.poisoned);
 CHECK(NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o)));CHECK(NT_SUCCESS(ParentMusbRelease((PVOID)&c)));
 Setup(&c,&bus);CHECK(NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7)));stuck=1;
 CHECK(!NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o))&&c.musb.held&&c.usb.leased);
 CHECK(!NT_SUCCESS(ParentMusbRelease((PVOID)&c))&&c.musb.mac);stuck=0;
 CHECK(NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o)));CHECK(NT_SUCCESS(ParentMusbRelease((PVOID)&c)));
 Setup(&c,&bus);CHECK(NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7)));external_voltage=1;gp[0x220/4]=1u<<9;macmem[0x60]=0x98;
 CHECK(NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o))&&o.source_off&&o.vbus_on); /* PC voltage not fabricated off */
 CHECK(NT_SUCCESS(ParentMusbRelease((PVOID)&c)));
 Setup(&c,&bus);verify_fail=1;
 CHECK(!NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7))&&c.musb.held&&c.usb.leased&&c.musb.poisoned&&!c.musb.mac);
 Setup(&c,&bus);CHECK(NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7)));oc_trip=1;
 CHECK(!NT_SUCCESS(ParentMusbVbus((PVOID)&c,TRUE))&&!(gp[0x110/4]&(1u<<6)));
 oc_trip=0;CHECK(NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o)));CHECK(NT_SUCCESS(ParentMusbRelease((PVOID)&c)));
 Setup(&c,&bus);CHECK(NT_SUCCESS(ParentMusbAcquire((PVOID)&c,7)));CHECK(NT_SUCCESS(ParentMusbVbus((PVOID)&c,TRUE)));
 off_stuck=1;CHECK(!NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o))&&c.musb.held&&c.usb.leased);
 CHECK(!NT_SUCCESS(ParentMusbRelease((PVOID)&c)));off_stuck=0;
 CHECK(NT_SUCCESS(ParentMusbStopAndProve((PVOID)&c,&o)));CHECK(NT_SUCCESS(ParentMusbRelease((PVOID)&c)));return 0;}
