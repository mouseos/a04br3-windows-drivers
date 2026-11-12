"""新規世代だけが、明示契約と実署名サービスによる自動回収を選べる。"""
import copy
import importlib.util
from pathlib import Path
import unittest

p = Path(__file__).resolve().parents[1] / 'tools/recovery/collection_reboot.py'
spec = importlib.util.spec_from_file_location('collection', p)
collection = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collection)


def policy(n=237):
    return dict(profile=f'combined-features-fresh{n}-signed-render-v1', service='test-service',
                data_directory='test-service', first_install_mask=0, deferred_unbound_mask=0,
                capture_selection=dict(capture_number=n),
                signed_render_contract=dict(terminal_windows_hold=False),
                diagnostic_clients=[dict(name='gpu_present', phase=1), dict(name='gpu_globaldiag', phase=1)],
                collection_reboot=copy.deepcopy(collection.CONFIG),
                unattended_collection_reboot=dict(schema='SIGNED_RENDER_UNATTENDED_COLLECTION_V1',
                    capture=n, bounded_winpe_required=True, require_atomic_publication=True))


class Contract(unittest.TestCase):
    def test_new_opt_in(self):
        p = policy()
        self.assertEqual(collection.policy_config(p), collection.CONFIG)
        self.assertIn('#define A04_COLLECTION_REBOOT 1', collection.macros(p))
        self.assertEqual(collection.admit(dict(collection_reboot=collection.CONFIG), p, True), collection.CONFIG)

    def test_old_capture_cannot_be_relabelled(self):
        with self.assertRaises(ValueError):
            collection.policy_config(policy(235))

    def test_missing_atomic_guard_rejected(self):
        p = policy()
        p['unattended_collection_reboot']['require_atomic_publication'] = False
        with self.assertRaises(ValueError):
            collection.policy_config(p)

    def test_terminal_hold_is_not_unattended(self):
        p = policy()
        p['signed_render_contract']['terminal_windows_hold'] = True
        with self.assertRaises(ValueError):
            collection.policy_config(p)

    def test_explicit_host_flag_required(self):
        with self.assertRaises(ValueError):
            collection.admit(dict(collection_reboot=collection.CONFIG), policy(), False)

    def test_legacy_default_stays_off(self):
        p = policy(235)
        del p['collection_reboot']
        del p['unattended_collection_reboot']
        p['signed_render_contract']['terminal_windows_hold'] = True
        self.assertIsNone(collection.policy_config(p))
        self.assertEqual(collection.macros(p), [])


if __name__ == '__main__':
    unittest.main()
