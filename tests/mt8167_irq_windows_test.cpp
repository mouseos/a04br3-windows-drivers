#include <cassert>
#include "irq_windows.h"
_KINTERRUPT test_interrupt{};
static int pending, fail, calls, fault_seen, total, starts, stops;
static uint32_t current;static int read_fail,reads;
static int read_cur(void *,uint32_t off,uint32_t *value) { assert(off==0x44);reads++;if(read_fail)return -1;*value=current;return 0; }
extern "C" int fake_dummy(void) { return 0; }
extern "C" int mt8167_dl1_interrupt(struct mt8167_dl1_engine *, int *notify) { if(fail)return -1; *notify=pending;return pending; }
extern "C" int mt8167_dl1_start(struct mt8167_dl1_engine *e,const struct mt8167_dl1_plan *) { starts++;e->state=MT8167_DL1_RUNNING;return 0; }
extern "C" int mt8167_dl1_stop(struct mt8167_dl1_engine *e) { stops++;e->state=MT8167_DL1_STOPPED;return 0; }
static void notify(void *, ULONG count, BOOLEAN fault) { calls++;total+=(int)count;fault_seen=fault; }
int main() {
 Mt8167Irq irq{}; mt8167_dl1_engine engine{};
 assert(Mt8167ConnectIrq(&irq,&engine,152,4,1,LevelSensitive,notify,nullptr)==0);
 fail=1;assert(!test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && !irq.dpc.queued && !irq.fault);fail=0;
 struct mt8167_dl1_plan plan{};plan.base=0x100000;plan.end=0x101fff;engine.io.read32=read_cur;
 uint32_t offset=99;assert(Mt8167SynchronizedPosition(&irq,&plan,&offset)==-1 && offset==99 && !reads);
 assert(!Mt8167SynchronizedStart(&irq,&plan) && starts==1);
 current=0x100100;assert(!Mt8167SynchronizedPosition(&irq,&plan,&offset) && offset==0x100);
 offset=99;current=0x102000;assert(Mt8167SynchronizedPosition(&irq,&plan,&offset)==-1 && offset==99);
 current=0x100001;assert(Mt8167SynchronizedPosition(&irq,&plan,&offset)==-1 && offset==99);
 read_fail=1;assert(Mt8167SynchronizedPosition(&irq,&plan,&offset)==-1 && offset==99);read_fail=0;
 pending=1; assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 assert(test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 irq.dpc.queued=0; irq.dpc.fn(&irq.dpc,irq.dpc.ctx,(PVOID)(ULONG_PTR)irq.generation,nullptr); assert(calls==1 && total==2 && !fault_seen);
 LONG pendingBefore=irq.pending;int callbacks=calls;
 irq.dpc.fn(&irq.dpc,irq.dpc.ctx,(PVOID)(ULONG_PTR)(irq.generation-1),nullptr);
 assert(irq.pending==pendingBefore && calls==callbacks);
 fail=1; assert(!test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && irq.dpc.queued);
 assert(Mt8167SynchronizedPosition(&irq,&plan,&offset)==-1 && offset==99);
 irq.dpc.queued=0; irq.dpc.fn(&irq.dpc,irq.dpc.ctx,(PVOID)(ULONG_PTR)irq.generation,nullptr); assert(calls==2 && fault_seen);
 assert(Mt8167SynchronizedStart(&irq,&plan)==-1 && starts==1);
 assert(!Mt8167SynchronizedStop(&irq) && stops==1);
 pending=1;fail=1;assert(!test_interrupt.fn(&test_interrupt,test_interrupt.ctx) && !irq.dpc.queued);
 Mt8167DisconnectIrq(&irq); assert(!irq.interrupt && !irq.dpc.queued && !test_interrupt.fn);
}
