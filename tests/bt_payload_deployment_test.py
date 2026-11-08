from pathlib import Path
import importlib.util,sys
S=Path(__file__).resolve().parents[1];sys.path.insert(0,str(S/'tools/winpe'))
spec=importlib.util.spec_from_file_location('media_builder',S/'tools/winpe/build-private-peripheral-batch-media.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
names=['mt6630-combo-bluetooth.inf','mt6630-combo-bluetooth.sys','mt6630-combo-bluetooth.cat'];cmd=m.bt_payload_prepare('MT8167CombinedFollowup146',{'files':[{'name':n} for n in names]})
dst='C:\\Windows\\System32\\MT8167CombinedFollowup146\\bt2013'
assert cmd[0].startswith('mkdir '+dst) and len([c for c in cmd if c.startswith('copy /b ')])==3
for name in names:
 copy=next(i for i,c in enumerate(cmd) if c.startswith('copy /b ') and c.split()[3]==dst+'\\'+name)
 assert cmd[copy+1]=='if errorlevel 1 goto candidatefail'
 assert cmd[copy+2].startswith('X:\\a04br3\\compare-files.exe ') and (dst+'\\'+name) in cmd[copy+2]
 assert cmd[copy+3]=='if errorlevel 1 goto candidatefail'
try:m.bt_payload_prepare('MT8167CombinedFollowup146',{'files':[{'name':'unknown.sys'}]})
except ValueError:pass
else:raise AssertionError('Unknown package must be rejected')
s=(S/'tools/winpe/build-private-peripheral-batch-media.py').read_text();assert s.index("prepare+=bt_payload_prepare(DIRECTORY,p['bt_first_install'])")<s.index("copy /b /y X:")
print('BT2013 normal-OS mkdir/copy/byte comparison before native arm: OK')
