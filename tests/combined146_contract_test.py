from pathlib import Path
import importlib.util,sys
S=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('p146',S/'tools/recovery/prepare-peripheral-followup146.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
assert p.SERVICE=='MT8167CombinedFollowup146' and p.CLIENT_NAMES==('audio_consumer',)
s=(S/'tools/recovery/prepare-peripheral-followup146.py').read_text(encoding='utf-8');assert "m['install_targets']==['audio']" in s and "m['gpu_client_execute'] is False" in s and "'--speaker-silence'" in s and '60000' in s
assert "'#define A04_RUN_THERMAL_READ 0'" in s and 'retained_wifi_actual' in s
r=(S/'tools/recovery/read-combined146-post.ps1').read_text(encoding='utf-8');assert "name -cne 'audio_consumer'" in r and '$p.gpu_client_count -ne 0' in r
ps=(S/'tools/recovery/prepare-offline-system-service.ps1').read_text(encoding='utf-8').split("if($ServiceName -ceq 'MT8167CombinedFollowup146'){")[1].split("}elseif($ServiceName -ceq 'MT8167CombinedFollowup144'){")[0]
assert "-cne 'audio'" in ps and 'gpu_client_count -ne 0' in ps and "'MT8167CombinedFollowup144'" in ps
print('fresh146 audio1034/BT payload fix/retainedGPU340 no execution/unfixedthermal no repeated IOCTL: OK')
