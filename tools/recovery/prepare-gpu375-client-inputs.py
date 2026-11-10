#!/usr/bin/env python3
"""fresh221/run222: client375 only. Shared373 generator, no profile copy."""
import importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('client_variants',Path(__file__).with_name('prepare-gpu373-client-inputs.py'))
m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.select_variant(221)
PROFILE=m.PROFILE;SERVICE=DIRECTORY=m.SERVICE
load_map=m.load_map;generate=m.generate;paths=m.paths;assert_paths=m.assert_paths
if __name__=='__main__':m.main()
