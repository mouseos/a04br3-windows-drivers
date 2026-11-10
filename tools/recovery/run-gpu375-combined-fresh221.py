#!/usr/bin/env python3
"""run222/fresh221/post223 explicit combined mode; same once-only latch/Observation."""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('shared_controller',Path(__file__).with_name('run-gpu368-fresh200.py'))
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':m.main('run222-combined',__file__)
