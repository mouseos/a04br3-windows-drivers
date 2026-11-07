/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Board sequence from CT3 7c478212 camera_hw/kd_camera_hw.c HI556 branch. */
#include "power_sequence.h"
int a04_hi556_power_start(const struct a04_hi556_power_ops *o,struct a04_hi556_power_state *s)
{
 if(!o||!s||s->attempted||!o->qualified||!o->reset||!o->pdn||!o->rail||!o->clock||!o->wait_ms||o->qualified(o->context)!=1)return -1;
#define STEP(expr) do{s->attempted++;s->raw=(expr);if(s->raw||o->qualified(o->context)!=1){s->uncertain=1;return -1;}}while(0)
 STEP(o->reset(o->context,1));STEP(o->pdn(o->context,0));
 STEP(o->rail(o->context,0,1800000));STEP(o->rail(o->context,1,2800000));STEP(o->rail(o->context,2,1200000));
 STEP(o->clock(o->context,24000000));STEP(o->pdn(o->context,1));STEP(o->wait_ms(o->context,1));
 STEP(o->reset(o->context,0));STEP(o->wait_ms(o->context,1));s->ready=1;return 0;
#undef STEP
}
