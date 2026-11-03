#include "button_hardware.h"
#include <assert.h>
#include <stdio.h>

struct mock { unsigned phase, writes, sent, last; uint16_t keys, status; int fail; };
static uint32_t mmio_read(void *p, unsigned reg)
{
    struct mock *m=p;
    if (reg==0x9c) return 1;
    assert(reg==0xa4);
    return m->phase ? (6u<<16) | m->status : 0;
}
static void mmio_write(void *p, unsigned reg, uint32_t value)
{
    struct mock *m=p;
    if (!m->phase) { assert(reg==0xa0 && value==0x00a10000); m->phase=1; }
    else { assert(reg==0xa8 && value==1); m->phase=0; }
    ++m->writes;
}
static void delay(void *p) { (void)p; }
static int keypad(void *p, unsigned reg, uint16_t *value)
{
    struct mock *m=p;
    assert(reg==4); *value=m->keys; return m->fail;
}
static int send(void *p, uint8_t button)
{
    struct mock *m=p; ++m->sent; m->last=button; return 0;
}
int main(void)
{
    struct mock m={0,0,0,0,3,6,0};
    struct a04_button_hardware h = {0};
    const uint8_t released[5]={0,0,0,0,0};
    h.context=&m; h.keypad_read16=keypad; h.notify=send;
    h.pmic.context=&m; h.pmic.read32=mmio_read; h.pmic.write32=mmio_write;
    h.pmic.delay=delay; h.pmic.attempts=3;
    a04_button_hardware_init(&h);
    assert(a04_button_device_sync_report(&h.device,released,5,1)==0);
    m.status=4; /* PMIC power pressed; keypad remains released. */
    assert(a04_button_device_poll(&h.device)==0);
    assert(a04_button_device_poll(&h.device)==0);
    assert(a04_button_device_poll(&h.device)==1 && m.sent==1 && m.last==0);
    m.fail=1; m.status=6;
    assert(a04_button_device_poll(&h.device)==0);
    assert(a04_button_device_poll(&h.device)==0);
    assert(a04_button_device_poll(&h.device)==1 && m.sent==2 && m.last==0);
    assert(m.writes==12);
    puts("Combined keypad and PMIC sampling verified");
    return 0;
}
