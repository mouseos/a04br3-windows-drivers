"""署名reportのbyte結合を検査する。合成PKCS7は暗号署名の成功を意味しない。"""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'atomic_lease', Path(__file__).resolve().parents[1] / 'tools/winpe/winpe_lease_package.py')
lease = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lease)


class SignatureAdmission(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.cert = b'\x30\x10synthetic-public-certificate'
        self.previous_thumb = lease.THUMB
        lease.THUMB = hashlib.sha1(self.cert).hexdigest().upper()
        (self.root / 'atomic-stage-replace.cer').write_bytes(self.cert)
        unsigned = bytearray(512)
        unsigned[:2] = b'MZ'
        struct.pack_into('<I', unsigned, 60, 128)
        unsigned[128:132] = b'PE\0\0'
        struct.pack_into('<H', unsigned, 132, 0x1c4)
        struct.pack_into('<H', unsigned, 152, 0x10b)
        marker = b'A04_ATOMIC_STAGE_V1:LEASE_QUERY:PRODUCTION\0'
        unsigned[384:384+len(marker)] = marker
        pkcs7 = b'\x30synthetic-pkcs7-for-report-tests:' + self.cert
        length = 8 + len(pkcs7)
        size = (length + 7) & ~7
        signed = unsigned + struct.pack('<IHH', length, 0x200, 2) + pkcs7 + bytes(size-length)
        struct.pack_into('<II', signed, 280, 512, size)
        self.exe = self.root / 'atomic-stage-replace.exe'
        self.exe.write_bytes(signed)
        reference = self.root / 'atomic-stage-replace-reference-x64.exe'
        reference.write_bytes(b'synthetic-reference-not-executed')
        (self.root / 'logs').mkdir()
        steps = []
        for name in ('sign-production', 'verify-production'):
            log = self.root / 'logs' / (name + '.log')
            log.write_bytes(('synthetic successful ' + name).encode())
            steps.append(dict(name=name, exit_code=0, log_sha256=lease.sha(log)))
        self.m = dict(
            schema='ATOMIC_STAGE_REPLACE_BUILD_V1', success=True, source_commit='a'*40,
            source_dirty=False, production_reference_mode=False, deadline_query_required=True,
            signed=True, production=self.row(self.exe), reference=self.row(reference), steps=steps,
            signing=dict(schema='ATOMIC_STAGE_AUTHENTICODE_V1', policy='Authenticode', digest='SHA256',
                         thumbprint=lease.THUMB, private_key_exported=False, new_identity_created=False,
                         public_certificate=self.row(self.root / 'atomic-stage-replace.cer'),
                         non_signature_payload=dict(unchanged=True, unsigned_bytes=512,
                             normalized_unsigned_sha256=hashlib.sha256(unsigned).hexdigest(),
                             normalized_signed_sha256=hashlib.sha256(unsigned).hexdigest(),
                             certificate_offset=512, certificate_bytes=size)),
            signatures=[dict(name=self.exe.name, status='Valid', thumbprint=lease.THUMB,
                             sha256=lease.sha(self.exe), certificate_sha256=hashlib.sha256(self.cert).hexdigest())])
        self.receipt = dict(schema='ATOMIC_STAGE_REPLACE_REFERENCE_TEST_V1', success=True,
            production_executed=False, reference_sha256=lease.sha(reference),
            production_sha256=lease.sha(self.exe),
            cases=[dict(name='synthetic-case-'+str(i), passed=True, actual_exit=0, expected_exit=0)
                   for i in range(20)])
        report = self.root / 'reference-result.json'
        report.write_text(json.dumps(self.receipt))
        self.m['reference_tests'] = dict(status='passed', reference_sha256=lease.sha(reference),
                                        cases=20, result_sha256=lease.sha(report))

    def tearDown(self):
        lease.THUMB = self.previous_thumb
        self.tmp.cleanup()

    @staticmethod
    def row(path):
        return dict(name=path.name, path=str(path), bytes=path.stat().st_size, sha256=lease.sha(path))

    def load(self):
        manifest = self.root / 'manifest.json'
        manifest.write_text(json.dumps(self.m))
        return lease.load_atomic_helper(self.root, manifest, lease.sha(manifest))

    def rebind_exe(self):
        self.m['production'] = self.row(self.exe)
        self.m['signatures'][0]['sha256'] = lease.sha(self.exe)

    def test_bound_signed_fixture_admitted(self):
        self.assertEqual(self.load()[1], self.exe)

    def test_legacy_unsigned_manifest_rejected(self):
        self.m['signed'] = False
        with self.assertRaisesRegex(ValueError, 'Signed production'):
            self.load()

    def test_missing_signed_claim_rejected(self):
        del self.m['signed']
        with self.assertRaisesRegex(ValueError, 'Signed production'):
            self.load()

    def test_signing_policy_identity_and_private_key_claims_rejected(self):
        for field, value in (('thumbprint', '0'*40), ('policy', 'other'),
                             ('digest', 'SHA1'), ('private_key_exported', True),
                             ('new_identity_created', True)):
            with self.subTest(field=field):
                original = copy.deepcopy(self.m)
                self.m['signing'][field] = value
                with self.assertRaisesRegex(ValueError, 'Existing BCBF'):
                    self.load()
                self.m = original

    def test_claim_without_certificate_directory_rejected(self):
        data = bytearray(self.exe.read_bytes())
        struct.pack_into('<II', data, 280, 0, 0)
        self.exe.write_bytes(data)
        self.rebind_exe()
        with self.assertRaisesRegex(ValueError, 'certificate directory'):
            self.load()

    def test_other_signer_or_invalid_status_rejected(self):
        for field, value in (('thumbprint', '0'*40), ('status', 'UnknownError'), ('sha256', '0'*64)):
            with self.subTest(field=field):
                original = copy.deepcopy(self.m)
                self.m['signatures'][0][field] = value
                with self.assertRaisesRegex(ValueError, 'actual bytes'):
                    self.load()
                self.m = original

    def test_missing_signature_report_rejected(self):
        self.m['signatures'] = []
        with self.assertRaisesRegex(ValueError, 'actual bytes'):
            self.load()

    def test_embedded_certificate_must_match(self):
        self.exe.write_bytes(self.exe.read_bytes().replace(self.cert, b'x'*len(self.cert)))
        self.rebind_exe()
        with self.assertRaisesRegex(ValueError, 'embedded BCBF'):
            self.load()

    def test_public_certificate_drift_rejected(self):
        (self.root / 'atomic-stage-replace.cer').write_bytes(b'other')
        with self.assertRaisesRegex(ValueError, 'certificate drift'):
            self.load()

    def test_failed_or_missing_signature_stage_rejected(self):
        for mode in ('failed', 'missing', 'duplicate'):
            with self.subTest(mode=mode):
                original = copy.deepcopy(self.m)
                if mode == 'failed': self.m['steps'][1]['exit_code'] = 1
                elif mode == 'missing': self.m['steps'].pop()
                else: self.m['steps'].append(copy.deepcopy(self.m['steps'][0]))
                with self.assertRaises(ValueError): self.load()
                self.m = original

    def test_actual_signature_log_drift_rejected(self):
        (self.root / 'logs/verify-production.log').write_bytes(b'altered')
        with self.assertRaisesRegex(ValueError, 'log drift'):
            self.load()

    def test_unsigned_receipt_cannot_bind_signed_exe(self):
        self.receipt['production_sha256'] = '0'*64
        report = self.root / 'reference-result.json'
        report.write_text(json.dumps(self.receipt))
        self.m['reference_tests']['result_sha256'] = lease.sha(report)
        with self.assertRaisesRegex(ValueError, 'actual production/reference'):
            self.load()

    def test_payload_change_after_signing_rejected_even_with_rebound_hash(self):
        data = bytearray(self.exe.read_bytes())
        data[350] ^= 1
        self.exe.write_bytes(data)
        self.rebind_exe()
        with self.assertRaisesRegex(ValueError, 'non-signature payload'):
            self.load()

    def test_dirty_source_rejected(self):
        self.m['source_dirty'] = True
        with self.assertRaisesRegex(ValueError, 'Clean atomic'):
            self.load()


if __name__ == '__main__':
    unittest.main()
