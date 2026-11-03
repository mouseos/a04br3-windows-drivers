#ifndef A04BR3_PMIC_READ_H
#define A04BR3_PMIC_READ_H
#include <stdint.h>

/* Caller must own WACS2 exclusively across the entire transaction, provide
 * ordered MMIO access, and keep the PMIC wrapper powered. A timed-out command
 * poisons this object until the hardware owner has performed recovery. */
struct a04_pmic_read {
    void *context;
    uint32_t (*read32)(void *, unsigned);
    void (*write32)(void *, unsigned, uint32_t);
    void (*delay)(void *);
    unsigned attempts;
    int poisoned;
};
int a04_pmic_chrstatus(struct a04_pmic_read *bus, uint16_t *value);
#endif
