"""Fresh196, GPU-only197 policy facade."""
import sys,importlib.util
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'recovery'))
_spec=importlib.util.spec_from_file_location('combined196_impl',Path(__file__).resolve().parents[1]/'recovery/prepare-peripheral-followup196.py');_impl=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_impl)
PROFILE=_impl.PROFILE;SERVICE=_impl.SERVICE;DIRECTORY=SERVICE
load_map=_impl.load_map;generate=_impl.generate;generate_paths=_impl.paths;assert_paths=_impl.assert_paths
