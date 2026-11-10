"""実fresh200/GPU368専用facade。旧196入口は変更しない。"""
import sys,importlib.util
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'recovery'))
s=importlib.util.spec_from_file_location('combined200_impl',Path(__file__).resolve().parents[1]/'recovery/prepare-peripheral-followup200.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
PROFILE=m.PROFILE;SERVICE=m.SERVICE;DIRECTORY=SERVICE
load_map=m.load_map;generate=m.generate;generate_paths=m.paths;assert_paths=m.assert_paths
