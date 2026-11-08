/* Execute actual BatteryClass callback logic. Locks are host stubs;
 * no WDF/IRP, IRQ, hardware or real temperature execution is claimed. */
#include <assert.h>
#include <stdio.h>
#include "../drivers/power/lc709203f/battery_class.h"
int main(void){
 struct a04_battery_class c={0};struct a04_gauge_sample s={3800,500,50};
 BATTERY_NOTIFY n={0,40,60};ULONG tag,old;BATTERY_STATUS bs;
 assert(a04_battery_set_notify(&c,1,&n)==STATUS_NO_SUCH_DEVICE);
 assert(a04_battery_publish(&c,&s));assert(!a04_battery_publish(&c,&s));
 assert(a04_battery_query_tag(&c,&tag)==STATUS_SUCCESS);
 assert(a04_battery_set_notify(&c,tag,&n)==STATUS_SUCCESS);
 assert(!a04_battery_publish(&c,&s));
 /* Both boundaries are inclusive; outside either bound triggers, even if
  * identical to the last published sample (registration race). */
 n.LowCapacity=50;n.HighCapacity=50;
 assert(a04_battery_set_notify(&c,tag,&n)==STATUS_SUCCESS);
 assert(!a04_battery_publish(&c,&s));
 n.LowCapacity=51;n.HighCapacity=100;
 assert(a04_battery_set_notify(&c,tag,&n)==STATUS_SUCCESS);
 assert(a04_battery_publish(&c,&s));assert(a04_battery_publish(&c,&s));
 assert(a04_battery_disable_notify(&c)==STATUS_SUCCESS);
 assert(!a04_battery_publish(&c,&s));
 n.LowCapacity=0;n.HighCapacity=49;
 assert(a04_battery_set_notify(&c,tag,&n)==STATUS_SUCCESS);
 assert(a04_battery_publish(&c,&s));
 n.LowCapacity=51;n.HighCapacity=49;
 assert(a04_battery_set_notify(&c,tag,&n)==STATUS_NOT_SUPPORTED);
 assert(c.notify_window.HighCapacity==49); /* invalid registration is atomic */
 n.LowCapacity=0;n.HighCapacity=0xffffffffu;n.PowerState=0x80000000u;
 assert(a04_battery_set_notify(&c,tag,&n)==STATUS_NOT_SUPPORTED);
 n.PowerState=BATTERY_POWER_ON_LINE|BATTERY_CHARGING;
 assert(a04_battery_set_notify(&c,tag,&n)==STATUS_SUCCESS);
 assert(a04_battery_publish(&c,&s)); /* published power differs */
 assert(a04_battery_publish_power(&c,&s,n.PowerState));
 assert(!a04_battery_publish_power(&c,&s,n.PowerState));
 assert(a04_battery_query_status(&c,tag,&bs)==STATUS_SUCCESS&&bs.PowerState==n.PowerState);
 old=tag;assert(a04_battery_invalidate(&c));assert(!c.notify_armed);
 assert(a04_battery_publish(&c,&s));assert(a04_battery_query_tag(&c,&tag)==STATUS_SUCCESS&&tag!=old);
 assert(a04_battery_set_notify(&c,old,&n)==STATUS_NO_SUCH_DEVICE);
 assert(a04_battery_set_notify(&c,tag,NULL)==STATUS_NOT_SUPPORTED);
 assert(a04_battery_disable_notify(&c)==STATUS_SUCCESS);
 puts("PASS inclusive windows, unchanged-outside registration race, power mismatch, cancellation, invalid atomicity, stale tags/D0 invalidation");
}
