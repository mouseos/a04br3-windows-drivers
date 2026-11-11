#!/usr/bin/env python3
"""Actual227/run228/post229. Existing once-only engine; terminal Windows hold."""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('engine',Path(__file__).with_name('run-gpu368-fresh200.py'));m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':m.main('run228',__file__)
