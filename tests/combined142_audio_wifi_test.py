from pathlib import Path
import importlib.util
R=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('scope142',R/'tools/recovery/prepare-peripheral-followup142.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
assert p.SERVICE=='MT8167CombinedFollowup142' and p.PROFILE=='combined-audio-classfix-fresh142-v1' and len(p.CLIENT_NAMES)==5
s=(R/'tools/recovery/prepare-peripheral-followup142.py').read_text();assert "m['install_targets']==['audio','wifi']" in s and "'boot_write_required':False" in s and "bm['source_fresh_manifest_sha256']==FRESH" in s
assert p.inf_class_guid((R/'drivers/audio/mt8167-afe/mt8167-afe-hp1033.inf').read_text()).lower()==p.TARGETS['audio'][2].lower()
ps=(R/'tools/recovery/prepare-offline-system-service.ps1').read_text().split("if($ServiceName -ceq 'MT8167CombinedFollowup142'){")[1].split("}elseif($ServiceName -ceq 'MT8167CombinedFollowup140'){")[0]
assert "-cne 'audio,wifi'" in ps and p.FRESH in ps and p.BOOT in ps and '$peripheralPreserve.gpu_client_count -ne 0' in ps
assert "'MT8167CombinedFollowup142':'MediaTek combined audio142'" in (R/'tools/winpe/private_hardware_probe_input.py').read_text()
assert '$policy.companion_packages.hcd1015' in (R/'tools/ci/build-combined142-service.ps1').read_text()
assert "cstr('hcd1015-import.bin')" in s and "cstr('hcd1015')" in s
c=(R/'tools/winpe/peripheral-batch-reused.h').read_text();assert 'if(mismatch){error=ERROR_REVISION_MISMATCH;goto done;}' in c and 'pnp_record(i,0x2a7' in c
print('fresh142 audio+wifi DIF/HCD1015 import-only/boot retain/GPU0/guard preserved: OK')
