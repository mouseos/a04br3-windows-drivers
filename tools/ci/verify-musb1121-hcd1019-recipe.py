#!/usr/bin/env python3
"""Verify source/input metadata only. Never runs Windows tools or a device."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re


def sha(data):
    return hashlib.sha256(data).hexdigest()


def closure(root, source, overrides=None):
    todo = [root / source]
    result = {}
    while todo:
        path = todo.pop().resolve()
        key = path.relative_to(root).as_posix()
        if key in result:
            continue
        data = (overrides or {}).get(key, path.read_bytes())
        result[key] = sha(data)
        for inc in re.findall(rb'^\s*#\s*include\s*"([^"\n]+)"', data, re.M):
            target = (path.parent / inc.decode()).resolve()
            assert target.is_relative_to(root) and target.is_file(), (key, inc)
            todo.append(target)
    return dict(sorted(result.items()))


def verify(root, lock, overrides=None):
    assert lock['schema'] == 'MT8167_MUSB1121_HCD1019_INPUT_CLOSURE_V1'
    assert lock['owner_ABI'] == 3
    rows = lock['compile']
    assert len(rows) == 12
    assert {(x['kind'], x['name']) for x in rows if x['kind'] == 'parent'} == {
        ('parent', 'parent'), ('parent', 'clock_snapshot')}
    assert len({x['name'] for x in rows if x['kind'] == 'hcd'}) == 10
    for row in rows + lock['host_sources']:
        assert closure(root, row['source'], overrides) == row['project_dependencies'], row['source']
    assert len(lock['host_sources']) == 18 and len(lock['host_tests']) == 7
    helpers = lock['reused_parent_helpers']
    assert len(helpers) == 13 and all(x['scope'] == 'parent' for x in helpers)
    for helper in helpers:
        assert re.fullmatch('[0-9a-f]{64}', helper['sha256'])
        for path, digest in helper['dependencies'].items():
            assert sha((overrides or {}).get(path, (root / path).read_bytes())) == digest, path
        assert not set(helper['dependencies']) & set(lock['changed_header_dependencies'])
    reverse = [dict(kind=x['kind'], name=x['name'], changed_headers=sorted(
        set(x['project_dependencies']) & set(lock['changed_header_dependencies'])))
        for x in rows if set(x['project_dependencies']) & set(lock['changed_header_dependencies'])]
    assert reverse == lock['affected_production_TUs']
    assert {(x['kind'], x['name']) for x in reverse} == {
        ('parent', 'parent'), ('hcd', 'musb_physical')}
    assert len(lock['sdk_headers']) == 2518
    for name, digest in lock['sdk_headers'].items():
        assert name.split('/')[0] in ('km', 'shared', 'um', 'wdf')
        assert '..' not in Path(name).parts and re.fullmatch('[0-9a-f]{64}', digest)
    for kind in ('parent', 'hcd'):
        pkg = lock['packages'][kind]
        data = (root / pkg['inf']).read_bytes()
        assert sha(data) == pkg['inf_sha256']
        assert b'\r\n' in data and b'\n' not in data.replace(b'\r\n', b'')
        text = data.decode()
        assert f'DriverVer=11/10/2026,{lock[kind+"_version"]}\r\n' in text
        assert f'CatalogFile={pkg["cat"]}\r\n' in text
        for dll, names in lock['rt81_contract']['baseline'][kind]['imports'].items():
            assert set(names) <= set(lock['rt81_contract']['actual_RT81_exports'][dll]['symbols'])
    evidence = lock['provenance']['hcd1018']
    assert sha((root / evidence['evidence_path']).read_bytes()) == evidence['recipe_sha256']
    # The copied historical recipe is read-only provenance, never invoked.
    builder = (root / 'tools/ci/build-mt8167-musb1121-hcd1019.ps1').read_text()
    assert evidence['evidence_path'] not in builder
    assert 'RequireHash (Join-Path $root $lock.provenance.hcd1018.evidence_path)' in builder
    assert not re.search(r'(?im)^\s*(?:&\s*)?(?:ssh|scp|gh|pnputil|devcon|msiexec|SetupDiCallClassInstaller)\b', builder)
    assert 'tests\\usb\\host_include' not in builder  # Windows model uses actual WDK headers
    assert '-DKMDF_VERSION_MAJOR=1' in builder and 'CheckClean' in builder
    assert 'StageOnly=$false;DIF_INSTALLDEVICE=$false' in builder
    assert 'HCD1018_adoption_assumed=$false' in builder
    assert 'if(!$BuildAndSign)' in builder
    # Lexical balance only: this is not a native PowerShell parser/runtime.
    stack = []
    quote = None
    comment = False
    i = 0
    while i < len(builder):
        c = builder[i]
        if comment:
            if c == '\n':
                comment = False
        elif quote:
            if c == '`' and quote == '"':
                i += 1
            elif c == quote:
                if i + 1 < len(builder) and builder[i + 1] == quote:
                    i += 1
                else:
                    quote = None
        elif c == '#':
            comment = True
        elif c in "'\"":
            quote = c
        elif c in '([{':
            stack.append(c)
        elif c in ')]}':
            assert stack and stack.pop() == {')': '(', ']': '[', '}': '{'}[c], i
        i += 1
    assert not stack and quote is None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--report', type=Path)
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    lock_path = root / 'tools/ci/musb1121-hcd1019-inputs.json'
    lock = json.loads(lock_path.read_text())
    verify(root, lock)
    rejected = []
    if options.self_test:
        for path in lock['changed_header_dependencies']:
            try:
                verify(root, lock, {path: (root / path).read_bytes() + b'\n/* drift */\n'})
            except AssertionError:
                rejected.append(path)
            else:
                raise AssertionError('Changed header escaped the source guard: ' + path)
        for bad in ('owner_ABI', 'helper_hash', 'INF_hash'):
            altered = copy.deepcopy(lock)
            if bad == 'owner_ABI':
                altered['owner_ABI'] = 2
            elif bad == 'helper_hash':
                altered['reused_parent_helpers'][0]['sha256'] = 'invalid'
            else:
                altered['packages']['hcd']['inf_sha256'] = '0' * 64
            try:
                verify(root, altered)
            except AssertionError:
                rejected.append(bad)
            else:
                raise AssertionError('Invalid metadata escaped the guard: ' + bad)
    report = dict(schema='MT8167_MUSB_RECIPE_SOURCE_VERIFICATION_V1',
                  closure_sha256=sha(lock_path.read_bytes()), ARM_compile_TUs=12,
                  unchanged_parent_helpers=13, host_compile_TUs=18, host_models=7,
                  SDK_header_hashes=2518, source_guard_rejections=len(rejected),
                  Windows_execution=False, native_PowerShell_parse=False,
                  runner_access=False, SDK_setup=False, hardware_verified=False)
    if options.report:
        # Reports belong to the shared project root, never an external path.
        project = root.parent.parent
        assert options.report.resolve().is_relative_to(project)
        options.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, separators=(',', ':')))


if __name__ == '__main__':
    main()
