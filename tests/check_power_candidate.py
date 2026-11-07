#!/usr/bin/env python3
"""One bounded host verification batch; never operates hardware or a runner."""
from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parents[1];out=root/'build/power-host-check';out.mkdir(parents=True,exist_ok=True)
commands=[['clang','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Idrivers/power/bq24160','-Idrivers/power/mt8167-thermal','drivers/power/bq24160/cold_profile.c','drivers/power/bq24160/charger.c','drivers/power/mt8167-thermal/boot_record.c','drivers/power/mt8167-thermal/thermal.c','tests/power_integration_test.c','-o',str(out/'power_integration_test')],[str(out/'power_integration_test')],['make','test-thermal','build/bq24160_charger_test','build/bq24160_i2c_test','build/lc709203f_test','build/lc709203f_i2c_test'],['build/bq24160_charger_test'],['build/bq24160_i2c_test'],['build/lc709203f_test'],['build/lc709203f_i2c_test']]
results=[]
with (out/'test.log').open('w') as log:
 for cmd in commands:
  r=subprocess.run(cmd,cwd=root,stdout=log,stderr=subprocess.STDOUT);results.append({'command':cmd,'exit':r.returncode})
  if r.returncode:break
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if results[-1]['exit']:raise SystemExit('Power verification failed; see build/power-host-check/test.log')
print('PASS one combined power-module verification batch (ASan/UBSan plus existing thermal/charger/gauge tests)')
