from pathlib import Path
import re,subprocess,json
S=Path(__file__).resolve().parents[2];paths=['drivers/gpu/mt8167/wddm-render/miniport.c','drivers/gpu/mt8167/wddm-render/display_adapter356.c','drivers/gpu/mt8167/wddm-display/bdd.cxx'];out=[]
for p in paths:
 old=subprocess.check_output(['git','-C',str(S),'show','efee1f9b:'+p],text=True);new=(S/p).read_text()
 new=new.replace('#include "../start_gap359.h"\n','').replace('GPU359-start-gap-v1','GPU358-segmented-cold-v1')
 new=re.sub(r'return Geo359End\(&gap,([^;]+)\);',r'return \1;',new)
 new=new.replace('struct geo_gap359_record gap;','')
 new=re.sub(r'Geo359(?:Begin|Enter|Result)\([^;]+\);','',new)
 new=re.sub(r'gap\.value\[\d+\]=[^;]+;','',new)
 new=new.replace('if(NT_SUCCESS(status)){}','')
 # The only extra block in Bridge guards observations of initialized POST info.
 def norm(x):return re.sub(r'\s+','',re.sub(r'/\*.*?\*/|//[^\n]*','',x,flags=re.S))
 assert norm(old)==norm(new),p
 out.append(p)
print('OBSERVATION_ONLY_CONTROL_RETURN_EQUIVALENCE',len(out))
