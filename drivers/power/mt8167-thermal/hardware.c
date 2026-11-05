/* SPDX-License-Identifier: GPL-2.0-only
 * Derived from the MT8167 Linux 4.4 thermal_fast_init, thermal_initial and
 * thermal_reset_and_initial sequences. Unvalidated on Windows or hardware. */
#include "hardware.h"
#include <stddef.h>
#define T 0x1100d000u
#define A 0x11003000u
#define I 0x10001000u
#define M 0x10018000u
#define WR(addr,val) do { if (h->write32(h->ctx,(addr),(val))) return -1; } while(0)
#define RD(addr,val) do { if (h->read32(h->ctx,(addr),&(val))) return -1; } while(0)
int a04_thermal_hw_init(const struct a04_thermal_hw *h)
{
 uint32_t v; unsigned i,j; const uint32_t msr[3]={0x90,0x94,0x98};
 if (!h || !h->qualified || !h->clocks_on || !h->read32 || !h->write32 || !h->delay_us || !h->qualified(h->ctx)) return -1;
 if(h->clocks_on(h->ctx)) return -1;
 WR(I+0x120,1); WR(I+0x124,1);
 RD(M+0x604,v); WR(M+0x604,v&~0x30u); if(h->delay_us(h->ctx,200)) return -1;
 /* Self-test the thermal AHB path using PTPSPARE2 as a synthetic ADC. */
 WR(T+0x428,0x1da1); WR(T+0x004,1); WR(T+0x008,1); WR(T+0x040,1);
 WR(T+0x044,0xff); WR(T+0x014,0); WR(T+0x018,0); WR(T+0x038,0);
 WR(T+0x048,0); WR(T+0x04c,1); WR(T+0x050,2);
 WR(T+0x064,T+0x420); WR(T+0x068,T+0x420); WR(T+0x074,T+0x424);
 WR(T+0x078,T+0x428); WR(T+0x07c,T+0x428); WR(T+0x080,0);
 WR(T+0x084,0x2c); WR(T+0x088,0); WR(T+0x08c,3); WR(T+0x00c,0); WR(T,0xf);
 for(i=0;i<3;i++) { for(j=0;j<100;j++) { RD(T+msr[i],v); if((v&0xfff)==0xda1) break; if(h->delay_us(h->ctx,2)) return -1; } if(j==100) return -1; }
 for(i=0;i<50;i++) { RD(T+0x418,v); if((v>>16)==0) break; if(h->delay_us(h->ctx,2)) return -1; }
 if(i==50) return -1;
 RD(T+0x03c,v); WR(T+0x03c,v|0x10e); WR(T,0);
 RD(A,v); WR(A,v&~0x800u); WR(A+0xc,0x800);
 WR(T+0x004,0x0c); WR(T+0x008,0x01c001c0); WR(T+0x040,0x0006fe8b);
 WR(T+0x038,0xdb); WR(T+0x044,0xffffffffu); WR(T+0x014,0); WR(T+0x018,0);
 WR(T+0x054,0x800); WR(T+0x068,A+0xc); WR(T+0x060,0x800); WR(T+0x074,A+8);
 WR(T+0x078,A+0x40); WR(T+0x07c,A+0x40); WR(T+0x080,0); WR(T+0x084,0x2c);
 WR(T+0x088,0); WR(T+0x048,0); WR(T+0x04c,1); WR(T+0x050,2);
 WR(T+0x064,M+0x604); WR(T+0x08c,3); WR(A+8,0x800); WR(T,0xf);
 RD(T+0x03c,v); WR(T+0x03c,v&~0x10eu);
 return 0;
}

/* Program the vendor's candidate SPM hot threshold only after initialization;
 * the platform must independently validate SPM routing and watchdog ownership. */
int a04_thermal_hw_protect(const struct a04_thermal_hw *h, const struct a04_thermal_cal *cal, int32_t hot_mc)
{
 uint16_t hot, cold; uint32_t interrupt;
 if(!h || !h->qualified || !h->initialized || !h->read32 || !h->write32 ||
    !h->qualified(h->ctx) || !h->initialized(h->ctx) ||
    a04_thermal_threshold(cal,hot_mc,&hot) || a04_thermal_threshold(cal,5000,&cold)) return -1;
 RD(T+0x00c,interrupt);
 WR(T+0x00c,0); WR(T+0x0c0,0x20000); WR(T+0x0c4,cold); WR(T+0x0cc,hot);
 WR(T+0x00c,interrupt|0x80000000u);
 return 0;
}

/* Linux MT8167 suspend first waits for the thermal AHB FSM, pauses sensing,
 * then disables periodic measurement. Leave shared clocks and analog rails
 * alone: another device may depend on them. Failure does not release a lease. */
int a04_thermal_hw_quiesce(const struct a04_thermal_hw *h)
{
 uint32_t v; unsigned i;
 if(!h || !h->qualified || !h->read32 || !h->write32 || !h->delay_us || !h->qualified(h->ctx))return -1;
 for(i=0;i<50;i++) {
  if(!h->qualified(h->ctx) || h->read32(h->ctx,T+0x418,&v) || v==UINT32_MAX)return -1;
  if(!(v>>16))break;
  if(h->delay_us(h->ctx,2))return -1;
 }
 if(i==50)return -1;
 if(!h->qualified(h->ctx) || h->read32(h->ctx,T+0x03c,&v) || v==UINT32_MAX)return -1;
 if(!h->qualified(h->ctx) || h->write32(h->ctx,T+0x03c,v|0x10eu))return -1;
 if(!h->qualified(h->ctx) || h->write32(h->ctx,T,0))return -1;
 if(!h->qualified(h->ctx) || h->read32(h->ctx,T,&v) || v!=0)return -1;
 for(i=0;i<50;i++) {
  if(!h->qualified(h->ctx) || h->read32(h->ctx,T+0x418,&v) || v==UINT32_MAX)return -1;
  if(!(v>>16))return h->qualified(h->ctx)?0:-1;
  if(h->delay_us(h->ctx,2))return -1;
 }
 return -1;
}
