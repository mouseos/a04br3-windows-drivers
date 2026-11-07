#include <cassert>
#include "wavert_registration.h"
const int CLSID_PortWaveRT=1;
static NTSTATUS new_status=0,init_status=0,register_status=0;
static int created,released,registered;
static WCHAR *retained_name;
struct Port : IPort {
 NTSTATUS Init(PDEVICE_OBJECT,PIRP,PUNKNOWN,PUNKNOWN,PRESOURCELIST) override { return init_status; }
 ULONG Release() override { released++;return 0; }
} port;
NTSTATUS PcNewPort(PPORT *out, const int &) { created++;if(new_status)return new_status;*out=&port;return 0; }
NTSTATUS PcRegisterSubdevice(PDEVICE_OBJECT,WCHAR *name,PUNKNOWN) {
 registered++;if(!register_status)retained_name=name;return register_status;
}
int main() {
 IMiniportWaveRT mini; void *p=&mini;
 assert(Mt8167RegisterWaveRt(nullptr,p,p,&mini)==STATUS_INVALID_PARAMETER && !created);
 new_status=-7;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==-7 && !released && !registered);
 new_status=0;init_status=-8;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==-8 && released==1 && !registered);
 init_status=0;register_status=-9;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==-9 && released==2 && registered==1);
 register_status=0;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==0 && released==3 && registered==2);
 // PortCls may use the borrowed name after StartDevice registration returns.
 assert(retained_name);
 const WCHAR expected[]=L"Mt8167WaveRT";
 const volatile WCHAR *borrowed=retained_name;
 for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++)assert(borrowed[i]==expected[i]);
}
