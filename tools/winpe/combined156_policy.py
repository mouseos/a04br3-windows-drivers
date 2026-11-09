"""run149 fresh148/12target final contract facade."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'recovery'))
import importlib.util
_spec=importlib.util.spec_from_file_location("combined156_impl",Path(__file__).resolve().parents[1]/"recovery/prepare-peripheral-followup156.py")
_impl=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_impl)
PROFILE=_impl.PROFILE;SERVICE=_impl.SERVICE;DIRECTORY=SERVICE
load_map=_impl.load_map;generate=_impl.generate;generate_paths=_impl.paths;assert_paths=_impl.assert_paths
