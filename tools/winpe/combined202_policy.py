"""実fresh202入力だけをGPU368共通生成器へ渡す。"""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('combined202_impl',Path(__file__).resolve().parents[1]/'recovery/prepare-peripheral-followup202.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
PROFILE=m.PROFILE;SERVICE=m.SERVICE;DIRECTORY=SERVICE
load_map=m.load_map;generate=m.generate;generate_paths=m.paths;assert_paths=m.assert_paths
