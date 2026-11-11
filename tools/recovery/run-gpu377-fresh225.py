#!/usr/bin/env python3
"""実fresh225/run226/render377。既once-only/terminal hold engine。"""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('engine',Path(__file__).with_name('run-gpu368-fresh200.py'))
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':m.main('run226',__file__)
