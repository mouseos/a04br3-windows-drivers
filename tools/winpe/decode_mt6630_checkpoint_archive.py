"""Import the fixed CLI decoder without executing it."""
import importlib.util
from pathlib import Path
_spec = importlib.util.spec_from_file_location('mt6630_fixed_checkpoint_decoder',
    Path(__file__).with_name('decode-mt6630-checkpoint.py'))
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
decode_record = _module.decode
