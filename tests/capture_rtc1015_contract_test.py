"""Check selective capture integration without hardware or a guessed parent."""
from pathlib import Path
import json,re,subprocess
r=Path(__file__).resolve().parents[1];base='13ae9f85c37331a3dd7324278e932956b9a840ef'
def old(p):return subprocess.check_output(['git','-C',str(r),'show',base+':'+p],text=True)
p='drivers/bus/mt8167-shared/wdf_parent_probe.c';s=(r/p).read_text()
intro='''#ifndef A04_SHARED_CAPTURE
#define A04_SHARED_CAPTURE 0
#endif
#if A04_SHARED_CAPTURE
#include "capture_input_owner.h"
#include "capture_interface.h"
#if !A04_SHARED_ENGINE_PCM || !A04_SHARED_FIRST_PLAYBACK
#error Capture requires the matched retained Engine parent profile
#endif
#endif
'''
assert s.count(intro)==1;s=s.replace(intro,'')
def off(m):
 body=m.group(1)
 assert '#if' not in body
 return body.split('#else\n',1)[1] if '#else\n' in body else ''
s=re.sub(r'#if A04_SHARED_CAPTURE\n(.*?)#endif\n',off,s,flags=re.S)
# Preserve strings/character literals while ignoring only C token whitespace.
lex=r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_][A-Za-z_0-9]*|\d+|==|!=|<=|>=|->|&&|\|\||\+\+|--|[^\s]'
assert re.findall(lex,s)==re.findall(lex,old(p)), 'Capture OFF must retain every old parent token'
unchanged=['drivers/usb','drivers/network','drivers/gpu','drivers/storage','drivers/bus/mt8167-shared/pwrap_owner.c','drivers/bus/mt8167-shared/pwrap_owner.h','drivers/bus/mt8167-shared/parent_resources.c','drivers/bus/mt8167-shared/parent_resources.h','drivers/bus/mt8167-shared/wifi_interface.h','drivers/bus/mt8167-shared/WIFI_RTC1015.md']
assert not subprocess.check_output(['git','-C',str(r),'diff',base,'--',*unchanged],text=True)
profile=json.loads((r/'drivers/bus/mt8167-shared/prototype/capture-rtc1015-profile.json').read_text())
expected=json.loads(old('drivers/bus/mt8167-shared/prototype/gpu-parent1014-spm-profile.json'))['flags']
expected['A04_SHARED_WIFI_RTC_BORROW']='1'
assert profile['flags']==expected
assert profile['source_commit']==base and profile['signed_sys_sha256']=='e7df942265a8216ba393152f1fe781e47d48ebe09b6da02f235dd8c2ba7d9752'
assert (r/p).read_text().count('WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04CaptureInterfaceGuid,NULL)')==1
print(json.dumps({'parent_capture_off_tokens_match_1015':True,'rtc_wifi_gpu_usb_sd_sources_unchanged':True,'existing_flag_count':len(expected),'borrow_preserved':True,'capture_forwarded_once':True}))
