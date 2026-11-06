#ifndef A04_PWRAP_OWNER_H
#define A04_PWRAP_OWNER_H
#include <stdint.h>
#include "../../input/a04br3-buttons/pmic_read.h"
/* All calls, including shutdown, take the same blocking lock. Parent owns its
 * lock and WACS2 mapping; callbacks must not reenter this object. The parent
 * must prevent object destruction while clients still hold references. */
struct a04_pwrap_owner {
    void *lock_context;
    int (*lock)(void *);
    void (*unlock)(void *);
    struct a04_pmic_read bus;
    int online;
};
/* Called only after the parent has claimed exclusive PnP resources and set up
 * its lock, bus callbacks and power. Publishing the interface is separate. */
int a04_pwrap_owner_start(struct a04_pwrap_owner *owner);
/* Stops admission while holding the same lock as the entire transaction. */
int a04_pwrap_owner_stop(struct a04_pwrap_owner *owner);
/* Narrow interface for the MT6392 button status register; no arbitrary PMIC
 * writes. Status is unchanged on failure. A timeout poisons the shared bus. */
/* Read-only fixed eMMC register set under the same owner lock. No regulator
 * writes; the parent must retain power and client references for the call. */
int a04_pwrap_owner_read_emmc_rails(struct a04_pwrap_owner *, struct a04_emmc_rails_raw *);
int a04_pwrap_owner_read_rtc32k(struct a04_pwrap_owner *, struct a04_rtc32k_raw *);
int a04_pwrap_owner_read_buttons(struct a04_pwrap_owner *owner, uint16_t *status);
/* Fixed raw speaker-clock observation under the same WACS2 lock; this is
 * neither an audio lease nor proof that the observed state will persist. */
int a04_pwrap_owner_read_speaker_clocks(struct a04_pwrap_owner *,uint16_t *);
/* Fixed, read-only MT6392 speaker register snapshot; not an audio lease. */
int a04_pwrap_owner_read_audio(struct a04_pwrap_owner *,struct a04_mt6392_audio_raw *);
#if defined(A04_MT6392_RTC32K_DIAGNOSTIC) && A04_MT6392_RTC32K_DIAGNOSTIC
/* Serialize each full RTC transaction against other parent WACS clients.
 * Caller must also establish ownership against firmware and other RTC writers. */
int a04_pwrap_owner_rtc32k_acquire(struct a04_pwrap_owner *,struct a04_rtc32k_lease *);
int a04_pwrap_owner_rtc32k_release(struct a04_pwrap_owner *,struct a04_rtc32k_lease *);
#endif
#endif
