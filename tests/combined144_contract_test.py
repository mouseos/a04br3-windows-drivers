from pathlib import Path
import importlib.util
S=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('profile144',S/'tools/recovery/prepare-peripheral-followup144.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
assert p.SERVICE=='MT8167CombinedFollowup144' and p.CLIENT_NAMES==('gpu_suite',)
assert p.TARGETS['wifi'][0]=='A04BR3\\MT6630SDIO' and p.TARGETS['wifi'][3]=='mt6630-combo-runtime.sys'
s=(S/'tools/winpe/mt8167-peripheral-update.c').read_text();assert s.index('DWORD final=combined_finalize(e)')<s.index('record(0,0x207,e)')<s.index('service_status.dwCurrentState=SERVICE_STOPPED')
assert 'i<A04_DIAGNOSTIC_CLIENT_COUNT' in s and 'first= A04_FEATURE144?0:readonly_one' in s
b=(S/'tools/winpe/combined144-bt.h').read_text();assert b.index('if(matches){e=ERROR_REVISION_MISMATCH;goto done;}')<b.index('SetupDiCreateDeviceInfoW(')
assert b.index('SetupDiSetDeviceRegistryPropertyW(')<b.index('DIF_REGISTERDEVICE')<b.index('DIF_INSTALLDEVICE')
assert 'return saved;' in b and 'proof.result=e;' in b
r=(S/'tools/recovery/read-combined144-post.ps1').read_text();assert '$readerClientCountSchema -ne 1' in r and 'ReadFixedRawValue($identity,\'ModuleIdentityV1\',3,144)' in r
print('fresh144 exact baseline / one final pixel client / BT first install / compact flush contract: OK')
