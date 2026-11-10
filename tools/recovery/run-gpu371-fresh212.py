#!/usr/bin/env python3
"""main専用run213/fresh212/post214。既engineとObservationをそのまま使う。"""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('gpu371_controller_engine',Path(__file__).with_name('run-gpu368-fresh200.py'))
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':m.main('run213',__file__)
