#ifndef AUDIO_NOTIFICATION_TEST_STUBS_H
#define AUDIO_NOTIFICATION_TEST_STUBS_H
#include <pthread.h>
#ifdef A04_TEST_MINIPORT
typedef unsigned char KIRQL;
#define PASSIVE_LEVEL 0
inline int KeGetCurrentIrql(){return 0;}
#endif
#define IO_NO_INCREMENT 0
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL (-20)
#endif
#ifndef STATUS_NOT_FOUND
#define STATUS_NOT_FOUND (-21)
#endif
struct KEVENT { unsigned signals=0; };typedef KEVENT *PKEVENT;
typedef pthread_mutex_t KSPIN_LOCK;
inline void KeInitializeSpinLock(KSPIN_LOCK *p){pthread_mutex_init(p,NULL);}
inline void KeAcquireSpinLock(KSPIN_LOCK *p,KIRQL *old){*old=0;pthread_mutex_lock(p);}
inline void KeReleaseSpinLock(KSPIN_LOCK *p,KIRQL){pthread_mutex_unlock(p);}
inline LONG KeSetEvent(PKEVENT e,int,BOOLEAN){return e->signals++;}
#endif
