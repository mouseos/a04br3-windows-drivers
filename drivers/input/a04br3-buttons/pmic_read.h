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
struct a04_emmc_rails_raw { uint16_t vemc_enable, vemc_selector, vio18_enable; };
/* Raw observations only: these values do not establish mute, trim, rail
 * safety or permission to enable a speaker. Caller serializes all reads. */
struct a04_mt6392_audio_rails { uint16_t vaud22, vaud22_selector, vaud28; };
/* Linux regulator enable and QI bits only; not a voltage measurement or a
 * guarantee that the rails remain powered. Caller holds the WACS2 lock. */
int a04_pmic_audio_rails(struct a04_pmic_read *,struct a04_mt6392_audio_rails *);
int a04_pmic_audio_rails_ready(const struct a04_mt6392_audio_rails *);
struct a04_mt6392_audio_raw { uint16_t spk_con0,spk_con9,spk_con11; };
/* Caller holds the same WACS2 owner lock across all three reads. Output is
 * unchanged on failure; values are register observations, not voltage proof. */
int a04_pmic_emmc_rails(struct a04_pmic_read *bus, struct a04_emmc_rails_raw *out);
int a04_pmic_chrstatus(struct a04_pmic_read *bus, uint16_t *value);
/* Read raw TOP_CKPDN1 only. Bits 1/2/3 are speaker PWM divider,
 * divider and clock PDN fields in the MT8167 MT6392 board header. This is
 * an observation, not an ownership or clock-rate guarantee. */
int a04_pmic_spk_clock_status(struct a04_pmic_read *,uint16_t *);
int a04_pmic_audio_observe(struct a04_pmic_read *bus,struct a04_mt6392_audio_raw *out);
#if defined(A04_MT6392_WRITE_DIAGNOSTIC) && A04_MT6392_WRITE_DIAGNOSTIC
/* Caller holds the common PWRAP lock; only Class D masks/values allowed.
 * SET/CLR aliases are rejected here and use a separate guarded lease. */
int a04_pmic_classd_supported(uint16_t,uint16_t,uint16_t);
int a04_pmic_classd_update(struct a04_pmic_read *,uint16_t,uint16_t,uint16_t);
int a04_pmic_classd_read_gain(struct a04_pmic_read *,uint16_t,uint16_t *);
#if defined(A04_MT6392_CLK_ALIAS_EXPERIMENT) && A04_MT6392_CLK_ALIAS_EXPERIMENT
/* Caller holds PWRAP lock and must independently prove exclusive ownership,
 * retained power and quiescence. Saved state is never cleared after failure. */
struct a04_pmic_spk_clock { uint16_t original; int held; };
int a04_pmic_spk_clock_acquire(struct a04_pmic_read *,struct a04_pmic_spk_clock *);
int a04_pmic_spk_clock_release(struct a04_pmic_read *,struct a04_pmic_spk_clock *);
#endif
#endif
#endif
