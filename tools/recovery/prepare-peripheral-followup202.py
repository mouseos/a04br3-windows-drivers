#!/usr/bin/env python3
"""実fresh202用の薄い入力設定。GPU368/200の実装と厳密検証を共用。"""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('gpu368_shared202',Path(__file__).with_name('prepare-peripheral-followup200.py'))
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
m.CAPTURE=202;m.PROFILE='combined-features-fresh202-gpu368-v1';m.SERVICE=m.DIRECTORY='MT8167CombinedFollowup202'
m.base.PROFILE=m.PROFILE;m.base.SERVICE=m.base.DIRECTORY=m.SERVICE
PROFILE=m.PROFILE;SERVICE=DIRECTORY=m.SERVICE
load_map=m.load_map;generate=m.generate;paths=m.paths;assert_paths=m.assert_paths
if __name__=='__main__':m.main()
