/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/bluetooth/mt6630/power_guard.h"
int main(void){
 int original_epoch;
 /* Before HCI OPEN WiFi may control its own cold-start. */
 assert(!a04_mt6630_combo_power_held(0,0));
 /* HCI begins before optional HFP/SCO exists: WLAN power/reset must not
  * disable the whole original combo epoch behind the live BTHX child. */
 assert(a04_mt6630_combo_power_held(&original_epoch,0));
 assert(a04_mt6630_combo_power_held(&original_epoch,1));
 /* A partial retained SCO hold remains authoritative even after file teardown. */
 assert(a04_mt6630_combo_power_held(0,1));
 /* Only no HCI file and no retained hold permits the previous cold path. */
 assert(!a04_mt6630_combo_power_held(0,0));
 return 0;
}
