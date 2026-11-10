#!/usr/bin/env python3
"""実fresh208からのGE8300タイミング更新。既状態機械で更新後Windowsを保持。"""
import importlib.util
from pathlib import Path

spec=importlib.util.spec_from_file_location('ge8300_timing_controller',Path(__file__).with_name('run-gpu368-fresh200.py'))
engine=importlib.util.module_from_spec(spec)
spec.loader.exec_module(engine)
if __name__=='__main__':
    engine.main('run209',__file__)
