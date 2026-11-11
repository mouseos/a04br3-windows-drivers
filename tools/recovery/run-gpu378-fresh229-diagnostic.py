#!/usr/bin/env python3
"""Actual229/run230 diagnostic-only, no driver DIF/restart; shared once-only engine."""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('engine',Path(__file__).with_name('run-gpu368-fresh200.py'));m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':m.main('run230',__file__)
