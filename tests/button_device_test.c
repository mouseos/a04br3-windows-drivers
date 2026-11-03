#include "button_device.h"
#include <assert.h>
#include <stdio.h>

struct mock { uint16_t keypad, pmic; int key_fail, pmic_fail, result; unsigned count, last; };
static int keys(void *p, uint16_t *v) { struct mock *m=p; *v=m->keypad; return m->key_fail; }
static int pmic(void *p, uint16_t *v) { struct mock *m=p; *v=m->pmic; return m->pmic_fail; }
static int notify(void *p, uint8_t v) { struct mock *m=p; ++m->count; m->last=v; return m->result; }
int main(void)
{
    struct mock m = {3, 6, 0, 0, 0, 0, 0};
    struct a04_button_device d = { {0}, &m, keys, pmic, notify };
    a04_button_device_init(&d);
    m.keypad=2; /* volume up pressed, but endpoint state not yet known */
    assert(!a04_button_device_poll(&d)); assert(!a04_button_device_poll(&d));
    assert(!a04_button_device_poll(&d) && !m.count);
    a04_button_device_sync(&d, 0);
    m.result=1; assert(!a04_button_device_poll(&d) && m.count==1 && m.last==2);
    m.result=0; assert(a04_button_device_poll(&d)==1 && m.count==2);
    m.keypad=3; m.key_fail=1;
    assert(!a04_button_device_poll(&d) && m.count==2);
    m.key_fail=0; m.result=-1;
    assert(!a04_button_device_poll(&d)); assert(!a04_button_device_poll(&d));
    assert(a04_button_device_poll(&d)==-1 && m.count==3);
    m.result=0; assert(!a04_button_device_poll(&d) && m.count==3);
    a04_button_device_sync(&d, 0); assert(!a04_button_device_poll(&d) && m.count==3);
    a04_button_device_disconnect(&d); assert(!a04_button_device_poll(&d));
    puts("Button delivery and unknown-result handling verified");
    return 0;
}
