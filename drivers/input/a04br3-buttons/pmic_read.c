/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pmic_read.h"

#define INIT_DONE2 0x9cu
#define WACS2_CMD 0xa0u
#define WACS2_RDATA 0xa4u
#define WACS2_VLDCLR 0xa8u
#define MT6392_CHRSTATUS 0x0142u

static int wait_fsm(struct a04_pmic_read *bus, unsigned state, uint32_t *last)
{
    unsigned i;
    for (i = 0; i < bus->attempts; ++i) {
        *last = bus->read32(bus->context, WACS2_RDATA);
        if (((*last >> 16) & 7u) == state) return 0;
        bus->delay(bus->context);
    }
    return -1;
}

static int read_register(struct a04_pmic_read *bus, uint16_t address, uint16_t *value)
{
    uint32_t data;
    uint16_t response;
    if (!bus || !value || !bus->read32 || !bus->write32 || !bus->delay ||
        !bus->attempts || bus->poisoned) return -1;
    if (!(bus->read32(bus->context, INIT_DONE2) & 1u)) return -1;
    if (wait_fsm(bus, 0, &data)) { bus->poisoned = 1; return -1; }
    bus->write32(bus->context, WACS2_CMD, ((uint32_t)address >> 1) << 16);
    if (wait_fsm(bus, 6, &data)) { bus->poisoned = 1; return -1; }
    response = (uint16_t)data;
    bus->write32(bus->context, WACS2_VLDCLR, 1);
    if (wait_fsm(bus, 0, &data)) { bus->poisoned = 1; return -1; }
    *value = response;
    return 0;
}

int a04_pmic_chrstatus(struct a04_pmic_read *bus, uint16_t *value)
{ return read_register(bus, MT6392_CHRSTATUS, value); }

int a04_pmic_audio_observe(struct a04_pmic_read *bus,struct a04_mt6392_audio_raw *out)
{
    struct a04_mt6392_audio_raw tmp;
    /* MT6392 SPK_CON0/9/11 from the board kernel register definitions. */
    if (!out || read_register(bus,0x0052u,&tmp.spk_con0) ||
        read_register(bus,0x0064u,&tmp.spk_con9) ||
        read_register(bus,0x0068u,&tmp.spk_con11)) return -1;
    *out=tmp;return 0;
}

int a04_pmic_emmc_rails(struct a04_pmic_read *bus, struct a04_emmc_rails_raw *out)
{
    struct a04_emmc_rails_raw tmp;
    if (!out || read_register(bus, 0x0508u, &tmp.vemc_enable) ||
        read_register(bus, 0x052eu, &tmp.vemc_selector) ||
        read_register(bus, 0x0556u, &tmp.vio18_enable)) return -1;
    *out=tmp;
    return 0;
}
