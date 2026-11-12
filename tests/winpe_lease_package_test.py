"""書込みより前に、無期限・別署名・改変されたWinPE候補を拒否する。"""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('lease_package', Path(__file__).resolve().parents[1] / 'tools/winpe/winpe_lease_package.py')
lease = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lease)


class Admission(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.package = self.root / 'package'
        self.package.mkdir()
        self.cert = b'isolated unit-test certificate fixture'
        self.old_thumb = lease.THUMB
        lease.THUMB = hashlib.sha1(self.cert).hexdigest().upper()
        binary = bytearray(512)
        binary[:2] = b'MZ'
        struct.pack_into('<I', binary, 60, 128)
        binary[128:132] = b'PE\0\0'
        struct.pack_into('<H', binary, 132, 0x1c4)
        binary[256:256+len(lease.MARKER)+1] = lease.MARKER + b'\0'
        for name in lease.NAMES:
            (self.package / name).write_bytes(bytes(binary) if name.endswith('.sys') else self.cert if name.endswith('.cer') else b'fixture')
        rows = [dict(name=p.name, bytes=p.stat().st_size, sha256=lease.sha(p)) for p in self.package.iterdir()]
        self.m = dict(schema='TOPRGU_WINPE_LEASE_PACKAGE_V1', success=True, signed=True,
                      lease_seconds=1800, force_swrst=True, hardware_verified=False,
                      source_commit='a'*40, source_dirty=False,
                      critical_flags=dict(A04_TOPRGU_WINPE_LEASE_SECONDS='1800', A04_TOPRGU_CPU1_HANDOFF='1',
                                          A04_TOPRGU_UART_SCREEN='1', A04_TOPRGU_UART_VOLUME='1',
                                          A04_TOPRGU_DEADLINE_SECONDS='0', A04_TOPRGU_THERMAL_STOP='0'),
                      certificate=dict(thumbprint=lease.THUMB, private_key_exported=False), outputs=rows,
                      signatures=[dict(name=r['name'], status='Valid', thumbprint=lease.THUMB, sha256=r['sha256']) for r in rows if r['name'].endswith(('.sys', '.cat'))],
                      steps=[dict(name=n, exit_code=0, log_sha256='a'*64) for n in ('verify-sys','verify-cat','member-mt8167-toprgu.sys','member-mt8167-toprgu.inf')])

    def tearDown(self):
        lease.THUMB = self.old_thumb
        self.tmp.cleanup()

    def load(self):
        manifest = self.root / 'manifest.json'
        manifest.write_text(json.dumps(self.m))
        return lease.load_package(self.root, manifest, lease.sha(manifest))

    def test_complete_fixture_admitted(self):
        self.assertEqual(self.load()[0]['lease_seconds'], 1800)

    def test_default_off_is_rejected(self):
        self.m['critical_flags']['A04_TOPRGU_WINPE_LEASE_SECONDS'] = '0'
        with self.assertRaisesRegex(ValueError, 'flag mismatch'):
            self.load()

    def test_unbounded_sys_with_claimed_flags_rejected(self):
        sys = self.package / 'mt8167-toprgu.sys'
        sys.write_bytes(sys.read_bytes().replace(lease.MARKER, b'x'*len(lease.MARKER)))
        row = next(r for r in self.m['outputs'] if r['name'] == sys.name)
        row['sha256'] = lease.sha(sys)
        with self.assertRaisesRegex(ValueError, 'marker missing'):
            self.load()

    def test_leaf_drift_rejected(self):
        (self.package / 'mt8167-toprgu.cat').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'leaf drift'):
            self.load()

    def test_signature_of_other_bytes_rejected(self):
        self.m['signatures'][0]['sha256'] = 'b'*64
        with self.assertRaisesRegex(ValueError, 'actual bytes'):
            self.load()

    def test_catalog_member_failure_rejected(self):
        self.m['steps'][-1]['exit_code'] = 1
        with self.assertRaisesRegex(ValueError, 'failed or duplicate'):
            self.load()

    def test_lease_manifest_drift_rejected(self):
        manifest = self.root / 'manifest.json'
        manifest.write_text(json.dumps(self.m))
        with self.assertRaisesRegex(ValueError, 'pin mismatch'):
            lease.load_package(self.root, manifest, '0'*64)


class AtomicStage(unittest.TestCase):
    def test_system_and_payload_keep_failure_checks(self):
        lines = [r'copy /b X:\a04br3\payload\render\ge8300-render.sys C:\Windows\System32\Stage\ge8300-render.sys >> "%A04_REPORT%" 2>&1',
                 'if errorlevel 1 goto candidatefail',
                 r'copy /b /y X:\a04br3\payload\SYSTEM.patched C:\Windows\System32\config\SYSTEM >> "%A04_REPORT%" 2>&1',
                 'if errorlevel 1 goto candidatefail']
        converted, count = lease.atomic_copy_stage(lines)
        self.assertEqual(count, 2)
        self.assertEqual(converted[1], lines[1])
        self.assertEqual(converted[3], lines[3])
        self.assertTrue(all('atomic-stage-replace.exe' in converted[i] for i in (0, 2)))

    def test_unknown_copy_cannot_bypass(self):
        with self.assertRaisesRegex(ValueError, 'cannot bypass'):
            lease.atomic_copy_stage([r'copy /b X:\unknown C:\Windows\System32\config\SYSTEM'])

    def test_no_system_publication_rejected(self):
        with self.assertRaisesRegex(ValueError, 'SYSTEM publication'):
            lease.atomic_copy_stage([])


if __name__ == '__main__':
    unittest.main()
