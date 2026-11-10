#!/usr/bin/env python3
"""main専用run219/fresh218/post220。terminal holdとcollect-onlyは共通engine。"""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('gpu373_controller',Path(__file__).with_name('run-gpu368-fresh200.py'))
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':m.main('run219',__file__)
