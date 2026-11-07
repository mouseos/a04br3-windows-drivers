"""RTC OFF時の親1026との一致と他担当範囲の不変性を検証。"""
from pathlib import Path
import re,subprocess,json
r=Path(__file__).resolve().parents[1];base='2d6231727e8ed72b6d7dca4148debc101ac5aeef'
def off(text):
 out=[];skip=0
 for line in text.splitlines(True):
  if not skip and re.match(r'#(?:if|ifndef)\b.*MT6392_RTC_CALENDAR',line):skip=1;continue
  if skip:
   if re.match(r'#\s*(?:if|ifdef|ifndef)\b',line):skip+=1
   elif re.match(r'#\s*endif\b',line):skip-=1
   continue
  out.append(line)
 assert not skip
 return ''.join(out)
def tokens(s):
 s=re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*',lambda m:'' if m[0].startswith(('/*','//')) else m[0],s,flags=re.S)
 return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_][A-Za-z_0-9]*|\d+|==|!=|<=|>=|->|&&|\|\||\+\+|--|[^\s]',s)
paths=['drivers/bus/mt8167-shared/'+p for p in ('wdf_parent_probe.c','pwrap_owner.c','pwrap_owner.h')]+['drivers/input/a04br3-buttons/pmic_read.'+x for x in ('c','h')]
for p in paths:
 old=subprocess.check_output(['git','-C',str(r),'show',base+':'+p],text=True)
 assert tokens(off((r/p).read_text()))==tokens(old),p
protected=['drivers/audio','drivers/usb','drivers/bluetooth','drivers/network','drivers/gpu','drivers/storage','drivers/bus/mt8167-shared/prototype','drivers/bus/mt8167-shared/gpu_parent_service.inc','drivers/bus/mt8167-shared/capture_parent.inc']
assert not subprocess.check_output(['git','-C',str(r),'diff',base,'--',*protected],text=True)
print(json.dumps({'rtc_off_parent1026_tokens_match':True,'protected_sources_unchanged':True,'rtc_mutations_all_gated':True}))
