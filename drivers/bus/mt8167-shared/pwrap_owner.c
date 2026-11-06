#include "pwrap_owner.h"
static int enter(struct a04_pwrap_owner *o)
{
    if (!o || !o->lock || !o->unlock) return -1;
    return o->lock(o->lock_context);
}
int a04_pwrap_owner_start(struct a04_pwrap_owner *o)
{
    int result = -1;
    if (enter(o)) return -1;
    if (!o->online && !o->bus.poisoned && o->bus.read32 && o->bus.write32 &&
        o->bus.delay && o->bus.attempts) { o->online = 1; result = 0; }
    o->unlock(o->lock_context);
    return result;
}
int a04_pwrap_owner_stop(struct a04_pwrap_owner *o)
{
    if (enter(o)) return -1;
#if defined(A04_MT6392_RTC32K_DIAGNOSTIC) && A04_MT6392_RTC32K_DIAGNOSTIC
    if (o->rtc_lease) { o->unlock(o->lock_context); return -1; }
#endif
    o->online = 0;
    o->unlock(o->lock_context);
    return 0;
}
int a04_pwrap_owner_read_buttons(struct a04_pwrap_owner *o, uint16_t *status)
{
    int result = -1;
    if (!status || enter(o)) return -1;
    if (o->online) result = a04_pmic_chrstatus(&o->bus, status);
    o->unlock(o->lock_context);
    return result;
}

int a04_pwrap_owner_read_emmc_rails(struct a04_pwrap_owner *o, struct a04_emmc_rails_raw *out)
{
    int result=-1;
    if (!out || enter(o)) return -1;
    if (o->online) result=a04_pmic_emmc_rails(&o->bus,out);
    o->unlock(o->lock_context);
    return result;
}

int a04_pwrap_owner_read_audio(struct a04_pwrap_owner *o,struct a04_mt6392_audio_raw *out)
{
    int result=-1;
    if (!out || enter(o)) return -1;
    if (o->online) result=a04_pmic_audio_observe(&o->bus,out);
    o->unlock(o->lock_context);return result;
}

int a04_pwrap_owner_read_speaker_clocks(struct a04_pwrap_owner *o,uint16_t *out)
{
    int result=-1;
    if (!out || enter(o)) return -1;
    if (o->online) result=a04_pmic_spk_clock_status(&o->bus,out);
    o->unlock(o->lock_context);return result;
}

int a04_pwrap_owner_read_rtc32k(struct a04_pwrap_owner *o, struct a04_rtc32k_raw *out)
{
    int result = -1;
    if (!out || enter(o)) return -1;
    if (o->online) result = a04_pmic_rtc32k(&o->bus, out);
    o->unlock(o->lock_context);
    return result;
}
#if defined(A04_MT6392_RTC32K_DIAGNOSTIC) && A04_MT6392_RTC32K_DIAGNOSTIC
int a04_pwrap_owner_rtc32k_acquire(struct a04_pwrap_owner *o,struct a04_rtc32k_lease *lease)
{
    int result=-1;
    if(!lease||enter(o))return -1;
    if(o->online && !o->rtc_lease && !lease->held && !lease->uncertain) {
        result=a04_pmic_rtc32k_acquire(&o->bus,lease);
        if(result && o->bus.poisoned)lease->uncertain=1;
        if(lease->held || lease->uncertain)o->rtc_lease=lease;
    }
    o->unlock(o->lock_context);return result;
}
int a04_pwrap_owner_rtc32k_release(struct a04_pwrap_owner *o,struct a04_rtc32k_lease *lease)
{
    int result=-1;
    if(!lease||enter(o))return -1;
    if(o->online && o->rtc_lease==lease) {
        result=a04_pmic_rtc32k_release(&o->bus,lease);
        if(!result)o->rtc_lease=0;
    }
    o->unlock(o->lock_context);return result;
}
#endif
