"""Keep the generation, native SCM metadata and no-GPU execution contract aligned."""
import ast,importlib.util,json,sys,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('combined138',R/'tools/recovery/prepare-peripheral-followup138.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
assert p.SERVICE=='MT8167CombinedFollowup138' and p.PROFILE=='combined-peripherals-fresh138-v1'
assert p.CLIENT_NAMES==('clock_compare','battery_temperature','accdet_status','afe_jack','audio_consumer')
ps=(R/'tools/recovery/prepare-offline-system-service.ps1').read_text();native=(R/'tools/winpe/private_hardware_probe_input.py').read_text();reader=(R/'tools/recovery/read-combined138-post.ps1').read_text()
assert "display_name='MediaTek combined peripherals138'" in ps and "'MT8167CombinedFollowup138':'MediaTek combined peripherals138'" in native
block=ps.split("if($ServiceName -ceq 'MT8167CombinedFollowup138'){")[1].split("}elseif($ServiceName -ceq 'MT8167CombinedFollowup136'){")[0]
assert p.PROFILE in block and p.FRESH in block and p.BOOT in block and "-cne 'parent,audio'" in block and '$peripheralPreserve.gpu_client_count -ne 0' in block
assert '$readerClientCountSchema' in reader and '$v.ClientCount.value -ne $readerClientCountSchema' in reader and p.PROFILE in reader
for n in ['tools/recovery/prepare-peripheral-followup138.py','tools/winpe/combined138_policy.py','tools/winpe/private_hardware_probe_input.py','tools/winpe/build-private-peripheral-batch-media.py']:ast.parse((R/n).read_text())
print('combined138 generation/native metadata/five-client no-GPU contract: OK')
