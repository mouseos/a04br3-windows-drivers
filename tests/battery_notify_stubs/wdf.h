typedef void *WDFWAITLOCK;
static inline void WdfWaitLockAcquire(WDFWAITLOCK l,void *t) {(void)l;(void)t;}
static inline void WdfWaitLockRelease(WDFWAITLOCK l) {(void)l;}
