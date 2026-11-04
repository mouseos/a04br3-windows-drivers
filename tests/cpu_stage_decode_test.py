import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / 'tools/cpu/decode-stage.py'
spec = importlib.util.spec_from_file_location('cpu_stage', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class CpuStageTest(unittest.TestCase):
    def test_known_lk_config_is_not_a_measurement(self):
        result = module.decode('mux=00000004 pll0=00000011 pll1=0009A000')
        self.assertEqual(result['configured_pll_hz'], 1001000000)
        self.assertIsNone(result['effective_cpu_hz'])

    def test_known_1300_config(self):
        self.assertEqual(module.decode('mux=00000004 pll0=00000011 pll1=000C8000')['configured_pll_hz'], 1300000000)

    def test_unknown_states_are_not_guessed(self):
        for sample in ('mux=00000000 pll0=00000011 pll1=0009A000',
                       'mux=00000004 pll0=00000010 pll1=0009A000',
                       'mux=00000004 pll0=00000011 pll1=0109A000',
                       'mux=00000004 pll0=00000011 pll1=00000000'):
            self.assertIsNone(module.decode(sample)['configured_pll_hz'])

    def test_missing_duplicate_or_truncated_values_are_rejected(self):
        for sample in ('mux=00000004 pll0=00000011',
                       'mux=00000004 mux=00000000 pll0=00000011 pll1=0009A000',
                       'mux=00000004 pll0=00000011 pll1=0009A0001'):
            with self.assertRaises(ValueError):
                module.decode(sample)


if __name__ == '__main__':
    unittest.main()
