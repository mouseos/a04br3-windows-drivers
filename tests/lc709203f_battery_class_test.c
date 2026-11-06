#include <assert.h>
#include "battery_class.h"
#include "battery_status.h"
int main(void)
{
    struct a04_battery_class c={0};
    struct a04_gauge_sample s={3800,525,50};
    BATTERY_STATUS status; ULONG tag;
    assert(a04_battery_publish(&c,&s));
    assert(a04_battery_query_tag(&c,&tag)==STATUS_SUCCESS);
    assert(a04_battery_query_status(&c,tag,&status)==STATUS_SUCCESS);
    assert(status.PowerState==0 && status.Capacity==50 && status.Voltage==3800);
    assert(a04_bq_status_charging(0x30) && a04_bq_status_charging(0x40));
    assert(!a04_bq_status_charging(0x50) && !a04_bq_status_charging(0x00));
    assert(a04_battery_publish_power(&c,&s,BATTERY_POWER_ON_LINE|BATTERY_CHARGING));
    assert(a04_battery_query_status(&c,tag,&status)==STATUS_SUCCESS);
    assert(status.PowerState==(BATTERY_POWER_ON_LINE|BATTERY_CHARGING));
    assert(!a04_battery_publish_power(&c,&s,BATTERY_POWER_ON_LINE|BATTERY_CHARGING));
    assert(a04_battery_publish_power(&c,&s,BATTERY_POWER_ON_LINE));
    assert(a04_battery_query_status(&c,tag,&status)==STATUS_SUCCESS && status.PowerState==BATTERY_POWER_ON_LINE);
    assert(a04_battery_invalidate(&c));
    assert(a04_battery_query_status(&c,tag,&status)==STATUS_NO_SUCH_DEVICE);
    assert(a04_battery_publish(&c,&s));
    assert(a04_battery_query_tag(&c,&tag)==STATUS_SUCCESS);
    assert(a04_battery_query_status(&c,tag,&status)==STATUS_SUCCESS && status.PowerState==0);
    return 0;
}
