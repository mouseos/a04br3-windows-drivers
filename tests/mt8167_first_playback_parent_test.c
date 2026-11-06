/* One integration fixture: actual PnP parent + WACS2 + clocks/codec callbacks. */
#define main legacy_parent_test_main
#include "mt8167_shared_wdf_test.c"
#undef main
struct first_pmic {uint16_t reg[32768],response;unsigned fsm,writes;int no_qi;} firstPmic;
static uint32_t firstRead(void *p,unsigned off) {
 struct first_pmic *f=p;if(off==0x9c)return 1;assert(off==0xa4);return f->fsm<<16|f->response;
}
static void firstWrite(void *p,unsigned off,uint32_t value) {
 struct first_pmic *f=p;unsigned reg;
 if(off==0xa8){f->fsm=0;return;}assert(off==0xa0);reg=((value>>16)&0x7fff)*2;
 assert(reg/2<sizeof(f->reg)/sizeof(f->reg[0]));
 if(value&0x80000000u) {
  uint16_t data=(uint16_t)value;f->writes++;
  if(reg==0x10c)f->reg[0x108/2]&=~data;
  else if(reg==0x10a)f->reg[0x108/2]|=data;
  else {f->reg[reg/2]=data;if((reg==0x404 || reg==0x424) && (data&0x4000) && !f->no_qi)f->reg[reg/2]|=0x8000;}
 }else {f->response=f->reg[reg/2];f->fsm=6;}
}
static void firstDelay(void *p){(void)p;}
int main(void) {
 unsigned before;uint32_t dac;memset(&testContext,0,sizeof(testContext));
 assert(!ParentAdd(NULL,NULL));
 testContext.topck=(void *)0x20000000;testContext.infra=(void *)0x20001000;
 testContext.gpio=(void *)0x20005000;testContext.pwrap=(void *)0x2000f000;
 testContext.apmixed=firstAnalog;testContext.apmixedLength=0x714;mockInit=1;
 testContext.pmic.bus.context=&firstPmic;testContext.pmic.bus.read32=firstRead;
 testContext.pmic.bus.write32=firstWrite;testContext.pmic.bus.delay=firstDelay;testContext.pmic.bus.attempts=3;
 firstPmic.reg[0x108/2]=0x800e;firstPmic.reg[0x54/2]=0x13;
 assert(!ParentD0(&testContext,0) && testContext.audio.online);
 assert(!audio.BindAfe(&testContext,mockAfeRegs,4096));
 before=firstPmic.writes;
 assert(audio.AcquireMuted(&testContext,44100)==-1 && !firstIdleRefs && firstPmic.writes==before);
 assert(!audio.AcquireMuted(&testContext,48000));
#if A04_SHARED_WIFI_RUNTIME
 {ULONG generation=0;
  assert(audio.Header.Version==11 && audio.ReadDiagnostic && wifi.Header.Version==2);
  assert(!wifi.Acquire(&testContext,&generation) && generation && testContext.wifiActive==1);
  assert(testContext.wifiRtc.held && testContext.pmic.rtc_lease==&testContext.wifiRtc);
  assert(testContext.audio.leased && firstIdleRefs==1 && testContext.regs.online);
 }
#endif
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
 { PVOID oldGpio=testContext.gpio;int oldWrites=writes;unsigned oldPmic=firstPmic.writes;
   testContext.gpio=usbGpio;usbGpio[0x220/4]=1u<<9; /* PC/ID-device, external VBUS */
   assert(usb.Acquire(&testContext)!=0 && !testContext.usb.leased && !testContext.usb.poisoned);
   assert(testContext.audio.leased && testContext.pmic.online && testContext.regs.online);
   assert(firstPmic.writes==oldPmic && writes==oldWrites);testContext.gpio=oldGpio;
 }
#endif
#if defined(A04_SHARED_ENGINE_PCM) && A04_SHARED_ENGINE_PCM
 {MDL pages={0,8192,{0x100,0x101}};
  assert(!audio.VerifyLivePcm(&testContext,&pages,0x100000,8192));
  assert(audio.VerifyLivePcm(&testContext,&pages,0x200000,8192));
  pages.pfns[1]=0x102;assert(audio.VerifyLivePcm(&testContext,&pages,0x100000,8192));
  assert(audio.VerifyDmaDrained(&testContext));
 }
#endif
 assert(testContext.audio.leased && testContext.audioPowerPinned && firstIdleRefs==1);
 assert(testContext.classd.active && testContext.trimCaptured && testContext.trimCode==0x13);
 assert((firstPmic.reg[0x404/2]&0xc000)==0xc000 && (firstPmic.reg[0x424/2]&0xc000)==0xc000);
 assert(!(firstPmic.reg[0x64/2]&0xf00) && !(firstPmic.reg[0x108/2]&0xe));
 assert(audio.AcquireMuted(&testContext,48000)==-1 && firstIdleRefs==1);
 assert(audio.ReleaseQuiescent(&testContext)==-1 && testContext.audio.leased);
 assert(!audio.PreparePlayback(&testContext,48000));
 assert(!audio.StartVoice(&testContext) && testContext.voice.active);
 assert((firstAnalog[0x708/4]&(15u<<9))==(9u<<9) && (firstAnalog[0x708/4]&(1u<<8)));
 /* Unmute cannot precede real DL1 enable. */
 assert(ParentFirstUnmuteReady(&testContext));
 dac=mockAfeRegs[0x10/4];mockAfeRegs[0x10/4]=dac|2;
 assert(!audio.UnmuteMin(&testContext) && (firstPmic.reg[0x64/2]&0xf00)==0x100);
 assert(!audio.Mute(&testContext) && !(firstPmic.reg[0x64/2]&0xf00));
 assert(ParentUnmap(&testContext)==STATUS_DEVICE_BUSY && firstIdleRefs==1 && testContext.audioAfe);
 assert(testContext.audio.leased && !testContext.audio.online);
 return 0;
}
