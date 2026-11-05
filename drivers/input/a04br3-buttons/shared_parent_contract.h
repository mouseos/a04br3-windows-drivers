/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BUTTON_SHARED_PARENT_CONTRACT_H
#define A04_BUTTON_SHARED_PARENT_CONTRACT_H
#include <stdint.h>
#include "../../bus/mt8167-shared/client_interface.h"
/* Raw identifies the keypad; only the paired translated range may be mapped.
 * PWRAP must never appear as a child-owned range in this configuration. */
static inline int a04_button_shared_keypad_resource(uint64_t raw,uint32_t rawLength,int rawExclusive,
    uint64_t translated,uint32_t translatedLength,int translatedExclusive)
{
    return raw==0x10002000u && rawLength==0x1000u && rawExclusive && translatedExclusive &&
        translatedLength==rawLength && translated<=UINT64_MAX-(translatedLength-1u);
}
struct a04_button_shared_selection { uint64_t translated; int seen; };
static inline int a04_button_shared_select(struct a04_button_shared_selection *s,
    uint64_t raw,uint32_t rawLength,int rawExclusive,uint64_t translated,uint32_t translatedLength,int translatedExclusive)
{
    if (!s || s->seen || !a04_button_shared_keypad_resource(raw,rawLength,rawExclusive,translated,translatedLength,translatedExclusive)) return 0;
    s->translated=translated;s->seen=1;return 1;
}
static inline int a04_button_shared_interface_valid(const A04_SHARED_INTERFACE *p)
{
    return p && p->Size==sizeof(*p) && p->Version==A04_SHARED_INTERFACE_VERSION && p->Context &&
        p->InterfaceReference && p->InterfaceDereference && p->ButtonPmicRead;
}
#endif
