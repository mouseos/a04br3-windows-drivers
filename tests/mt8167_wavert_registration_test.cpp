#include <cassert>
#include "wavert_registration.h"
const int CLSID_PortWaveRT=1;
static NTSTATUS new_status=0,init_status=0,register_status=0;
static int created,released,registered;
struct Port : IPort {
 NTSTATUS Init(PDEVICE_OBJECT,PIRP,PUNKNOWN,PUNKNOWN,PRESOURCELIST) override { return init_status; }
 ULONG Release() override { released++;return 0; }
} port;
NTSTATUS PcNewPort(PPORT *out, const int &) { created++;if(new_status)return new_status;*out=&port;return 0; }
NTSTATUS PcRegisterSubdevice(PDEVICE_OBJECT,WCHAR *,PUNKNOWN) { registered++;return register_status; }
int main() {
 IMiniportWaveRT mini; void *p=&mini;
 assert(Mt8167RegisterWaveRt(nullptr,p,p,&mini)==STATUS_INVALID_PARAMETER && !created);
 new_status=-7;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==-7 && !released && !registered);
 new_status=0;init_status=-8;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==-8 && released==1 && !registered);
 init_status=0;register_status=-9;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==-9 && released==2 && registered==1);
 register_status=0;assert(Mt8167RegisterWaveRt(p,p,p,&mini)==0 && released==3 && registered==2);
}
