/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_CLEAR_FIXTURE_H
#define A04_GE8300_CLEAR_FIXTURE_H
#include "pvr_rogue_fwif_client.h"
/* Caller supplies two separate zeroed 4KiB pages at GPU VAs 0xda00002000
 * and 0xe000005000. The command writes a 16KiB RGBA8 image mapped at
 * 0x80000b6000. Fixed Mesa-generated GE8300 test; no external commands. */
int a04_gpu_clear_fixture(void *pds,void *usc,struct rogue_fwif_cmd_transfer *command);
#endif
