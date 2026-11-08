/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT6630_TRANSPORT_CAPS_H
#define MT6630_TRANSPORT_CAPS_H
/* Include broker_abi.h first. Strictly consume the separate live route proof;
 * this does not grant an HFP provider, stream, PCM/latency value or DMA lease. */
struct a04_mt6630_route_facts {
 unsigned merge,firmware_ready,firmware_fault,stp_ready,phase_ready,commands,acked;
 unsigned radio_ready,version_valid,pcm_ack,epoch_live;
};
static int a04_mt6630_route_ready(const struct a04_mt6630_route_facts *f)
{
 return f&&f->merge==1&&f->firmware_ready==1&&!f->firmware_fault&&
  f->stp_ready==1&&f->phase_ready==1&&f->commands&&f->commands==f->acked&&
  f->radio_ready==1&&f->version_valid==1&&f->pcm_ack==1&&f->epoch_live==1;
}
static int a04_mt6630_transport_bypass(const struct a04_bt_transport_caps *c)
{return c&&c->version==1&&c->ready==1&&c->generation&&c->routes==A04_BT_ROUTE_HCI_BYPASS;}
#endif
