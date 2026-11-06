#!/usr/bin/env python3
"""Reject modified candidate bytes, evidence, and certificates before staging."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
workspace=root.parents[1]
source=Path(os.environ['A04_TX_CANDIDATES']).resolve(strict=True)
verifier=Path(os.environ['A04_TX_OSSLSIGNCODE']).resolve(strict=True)
with tempfile.TemporaryDirectory(prefix='transaction-guard-test-',dir=workspace/'.codex-work') as tmp:
    base=Path(tmp);copy=base/'candidate';shutil.copytree(source,copy,ignore=shutil.ignore_patterns('scratch'))
    def run(name,success):
        result=subprocess.run(['python3',str(root/'tools/winpe/prepare-transaction-trust.py'),'--candidate',str(copy),'--osslsigncode',str(verifier),'--output',str(base/name)],capture_output=True,text=True)
        if (result.returncode==0)!=success:raise AssertionError(name+': '+result.stderr[-1000:])
    run('valid',True)
    target=copy/'touch/bu21025.inf';original=target.read_bytes();target.write_bytes(original+b'changed');run('bad-inf',False);target.write_bytes(original)
    target=copy/'parent/signer.cer';original=target.read_bytes();target.write_bytes(original+b'changed');run('bad-cert',False);target.write_bytes(original)
    target=copy/'catalog-proof.json';original=target.read_bytes();target.write_bytes(b'{}');run('bad-proof',False);target.write_bytes(original)
    target=copy/'manifest.json';original=target.read_bytes();target.write_bytes(original.replace(b'66b1db86',b'00b1db86'));run('bad-source',False)
print('TRANSACTION_CANDIDATE_GUARD_PASS')
