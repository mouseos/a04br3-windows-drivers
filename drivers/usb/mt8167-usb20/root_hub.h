/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_ROOT_HUB_H
#define A04_ROOT_HUB_H
#include "host_core.h"
#if A04_MUSB_HOST_CORE_EXPERIMENT
#define A04_HUB_OK 0
#define A04_HUB_STALL -1
#define A04_HUB_PENDING 1
/* Serialized by the exclusive HCD owner. No MMIO or physical power writes.
 * POWER and RESET return actions, never acknowledge hardware success.
 * Owner must quiesce transfers before acknowledging either action. */
struct a04_hub {
 struct a04_root_port *port;
 unsigned action, generation, active_generation;
 unsigned char power_good_2ms;
};
struct a04_hub_action { unsigned kind, generation; };
#define A04_HUB_POWER_ON 1
#define A04_HUB_POWER_OFF 2
#define A04_HUB_RESET 3
/* Observations must come from the board owner, never from USB request data. */
struct a04_hub_observation {
 int qualified, id_host, overcurrent_clear, vbus_verified, vbus_off_verified, endpoints_quiesced;
 unsigned reset_asserted_ms, speed;
};
int a04_hub_init(struct a04_hub *,struct a04_root_port *,unsigned power_good_2ms);
int a04_hub_control(struct a04_hub *,const unsigned char setup[8],
 unsigned char *buffer,unsigned capacity,unsigned *actual,struct a04_hub_action *);
int a04_hub_action_done(struct a04_hub *,const struct a04_hub_action *,int success,
 const struct a04_hub_observation *);
/* Interrupt endpoint bitmap: pending until a change is latched; does not clear. */
int a04_hub_status(const struct a04_hub *,unsigned char *bitmap,unsigned capacity);
/* Invalidate outstanding actions on disconnect, overcurrent, stop or remove. */
void a04_hub_invalidate(struct a04_hub *);
#endif
#endif
