from pathlib import Path
import importlib.util
R=Path(__file__).resolve().parents[1]
for n in [138,140]:
 spec=importlib.util.spec_from_file_location('class_profile'+str(n),R/f'tools/recovery/prepare-peripheral-followup{n}.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
 text=(R/'drivers/audio/mt8167-afe/mt8167-afe-hp1033.inf').read_text();assert p.inf_class_guid(text).lower()==p.TARGETS['audio'][2].lower()
 wrong=text.replace('08002be10318','08002be10338');assert p.inf_class_guid(wrong).lower()!=p.TARGETS['audio'][2].lower()
 for invalid in ['[Version]\nClassGuid=wrong\n','[Version]\nClassGuid={4d36e96c-e325-11ce-bfc1-08002be10318}\nClassGuid={4d36e96c-e325-11ce-bfc1-08002be10318}\n']:
  try:p.inf_class_guid(invalid)
  except ValueError:pass
  else:raise AssertionError('ambiguous/invalid class accepted')
print('1033 INF MEDIA class / wrong338 and ambiguous rejection: OK')
