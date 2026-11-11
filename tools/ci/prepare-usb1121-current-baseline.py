#!/usr/bin/env python3
"""USB単体更新用の実取得baseline。既GPUを更新・再ラベルしない。"""
import argparse
import copy
import importlib.util
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('usb_component', HERE / 'prepare-usb1121-service198.py')
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
b = component.b
ROLES = {'rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render','msdc','bt'}
ROLE_SYS = dict(zip(('rtc','thermal','usb','wifi','sd','display','parent','keypad','battery','audio','render','msdc','bt'),
                   ('mt6392-rtc.sys','mt8167-thermal.sys','mt8167-usb20.sys','mt6630-combo-runtime.sys',
                    'mt8167-sd.sys','mt8167-display-transport.sys','mt8167-shared.sys','mt8167-keypad.sys',
                    'lc709203f.sys','mt8167-audio.sys','ge8300-render.sys','mt8167-msdc.sys','mt6630-combo-bluetooth.sys')))


def pinned(root, ref):
    b.require(isinstance(ref, dict) and set(ref) == {'path','sha256'} and
              isinstance(ref['path'], str) and isinstance(ref['sha256'], str) and
              re.fullmatch('[0-9a-f]{64}', ref['sha256'] or ''), '実path/SHA pinが必要')
    p = b.below(root, ref['path'])
    b.require(b.sha(p) == ref['sha256'], '入力pin不一致')
    return p, b.read(p)


def build(root, inputs, *, for_deployment=False):
    b.require(set(inputs) == {'schema','fresh','native','supplement','actual_package_rows','GPU_manifest',
                             'no_intervening_device_write_reported_by_main'} and
              inputs['schema'] == 'USB1121_CURRENT_GPU_BASELINE_INPUTS_V1', '明示的な実baseline mapが必要')
    fp, fresh = pinned(root, inputs['fresh'])
    np, native = pinned(root, inputs['native'])
    sp, supplement = pinned(root, inputs['supplement'])
    _, packages = pinned(root, inputs['actual_package_rows'])
    gp, _ = pinned(root, inputs['GPU_manifest'])
    no_write = inputs['no_intervening_device_write_reported_by_main']
    b.require(type(no_write) is bool and (not for_deployment or no_write), '配備には取得後の書込みなしの主担当記録が必要')
    b.require(type(native.get('capture_number')) is int and native['capture_number'] > 0 and
              native.get('privilege_restore_error') == 0, '実capture番号とnative cleanup成功が必要')
    b.require(supplement.get('source_manifest_sha256') == inputs['fresh']['sha256'] and
              supplement.get('recovery_boot_id') == fresh['recovery_boot_id'], '同freshの補足だけを使用')
    files = {}
    for mp, manifest in ((fp, fresh), (sp, supplement)):
        b.require(manifest.get('status') == 'captured-unmounted' and manifest.get('read_only') is True and
                  manifest.get('unmounted') is True and manifest.get('errors') == [], '完了済みread-only取得が必要')
        for item in manifest['files']:
            name = item['source_path']
            b.require(not Path(name).is_absolute() and ':' not in name and '\\' not in name and
                      '..' not in Path(name).parts and item['backup_path'] == 'files/' + name,
                      '取得path不正')
            p = b.below(root, mp.parent / item['backup_path'])
            b.require(p.is_relative_to(mp.parent) and p.stat().st_size == item['bytes'] and
                      b.sha(p) == item['sha256'], '取得原本byte不一致')
            b.require(name not in files or files[name] == item['sha256'], '重複取得の不一致')
            files[name] = item['sha256']
    b.require(set(packages) == ROLES, '全13roleの実package行が必要')
    for role, row in packages.items():
        b.require(set(row['names']) == {'inf','sys','cat'} and set(row['hashes']) == {'inf','sys','cat'}, 'trio不完全')
        b.require(row['names']['sys'] == ROLE_SYS[role], 'roleと実ハードSYS名が不一致')
        mp = b.below(root, row['manifest'])
        b.require(b.sha(mp) == row['manifest_sha256'], '保持packageの出自不一致')
        manifest = b.read(mp)
        source = manifest.get('package_source_commit', manifest.get('source_commit'))
        if role == 'display':
            source = manifest.get('new_display_source_commit', source)
        b.require(isinstance(source, str) and re.fullmatch('[0-9a-f]{40}', source) and
                  source == row['source_commit'], 'roleのmanifest sourceが不一致')
        members = manifest.get('files', manifest.get('outputs', []))
        b.require(isinstance(members, list) and members, '実manifestのleaf集合が必要')
        for kind, name in row['names'].items():
            b.require(re.fullmatch('[A-Za-z0-9_.-]+', name), 'package basename不正')
            b.require(b.sha(b.below(root, Path(row['directory']) / name)) == row['hashes'][kind], '保持package byte不一致')
            matches = [f['sha256'].lower() for f in members
                       if Path(f.get('path', f.get('name','')).replace('\\','/')).name == name]
            b.require(matches == [row['hashes'][kind]], 'roleのtrioとmanifest leafが不一致')
        b.require(files.get('Windows/System32/Drivers/' + row['names']['sys']) == row['hashes']['sys'],
                  '取得した実SYSとpackageが不一致')
    extras = packages['render'].get('extra_members', [])
    expected = {'ge8300-umd.dll':'ge8300-umd.dll', 'ge8300-mips.fw':'Drivers/ge8300-mips.fw'}
    b.require(len(extras) == len(expected) and
              {x['name']:x['installed'].replace('\\','/') for x in extras} == expected,
              '実GPUの必須UMD/FW installed集合が不完全')
    raw_values = []
    for path, value in native['captured_named_values'].items():
        key, sep, name = path.rpartition('\\')
        b.require(bool(sep) and bool(key), 'native value path不正')
        raw_values.append(dict(key=key, name=name, type=value['type'], raw_hex=value['raw_hex']))
    policy = dict(schema='USB1121_CAPTURED_CURRENT_GPU_POLICY_V1',
                  profile='usb1121-current-gpu-baseline-v1', draft=False, normal_install_allowed=True,
                  backup_manifest_sha256=inputs['fresh']['sha256'], recovery_boot_id=fresh['recovery_boot_id'],
                  service_preservation=copy.deepcopy(native['old_services']),
                  captured_registry_values=raw_values, packages=copy.deepcopy(packages),
                  install_targets=['usb1121-component'], GPU_updated=False, hardware_verified=False,
                  baseline_inputs=copy.deepcopy(inputs))
    comparison = b.admit_native(root, fp, inputs['fresh']['sha256'], np, inputs['native']['sha256'], policy)
    binding, retained = component.bind_gpu(root, policy, gp, inputs['GPU_manifest']['sha256'])
    for name, ref in retained.items():
        b.require(files.get(name) == ref['sha256'], '将来GPUの先取り禁止。実取得GPUの全SYS/UMD/FWが必要')
    for role, hwid in (('render','ACPI\\MOU0006'), ('display','ACPI\\MOU0007')):
        node = native['nodes'][hwid]
        b.require(node['present'] is True and len(node['instances']) == 1, '実GPU nodeが一意でない')
        selected = node['instances'][0]['selected']
        b.require(selected['DriverVersion']['value'] == packages[role]['driver_version'] and
                  selected['InfPath']['value'].casefold() == packages[role]['names']['inf'].casefold(),
                  'native選択と実GPU packageが不一致')
    policy.update(current_GPU_binding=binding, native_comparison=comparison,
                  copied_package_rows_are_current_file_witnesses=True)
    return policy


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--workspace', type=Path, required=True)
    ap.add_argument('--map', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--finalize', action='store_true')
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)
    source = b.below(root, a.map)
    source_hash = b.sha(source)
    policy = build(root, b.read(source), for_deployment=a.finalize)
    out = a.out.resolve()
    b.require(out.is_relative_to(root) and not out.is_relative_to(HERE.parents[1]) and not out.exists(), 'source外・root内の新規出力が必要')
    if a.finalize:
        ref = policy['baseline_inputs']['fresh']
        fresh_path = b.below(root, ref['path'])
        fresh = b.read(fresh_path)
        # 実機の同Recovery世代・対象・unmounted状態を既存入口で再確認。
        # これはRecovery内の介在書込みを検出する全ディスク監査ではない。
        subprocess.run([sys.executable, '-B', str(HERE.parents[0] / 'recovery/capture-windows-prebackup.py'),
                        'verify', '--serial', fresh['serial'], '--manifest', str(fresh_path),
                        '--manifest-sha256', ref['sha256']], check=True)
    b.require(b.sha(source) == source_hash, '生成中に入力mapが変更された')
    out.mkdir(parents=True)
    result = dict(schema='USB1121_CURRENT_GPU_BASELINE_REVIEW_V1',
                  input_map=dict(path=source.relative_to(root).as_posix(), sha256=source_hash),
                  GPU_updated=False, hardware_verified=False, deployment_policy_generated=a.finalize,
                  live_same_recovery_verified=a.finalize,
                  no_intervening_device_write_reported_by_main=policy['baseline_inputs']['no_intervening_device_write_reported_by_main'],
                  current_GPU_version=policy['current_GPU_binding']['target_gpu_version'],
                  capture_number=b.read(b.below(root, policy['baseline_inputs']['native']['path']))['capture_number'],
                  native_comparison=policy['native_comparison'])
    (out / 'baseline-review.json').write_text(json.dumps(result, indent=2) + '\n')
    if a.finalize:
        (out / 'current-policy.json').write_text(json.dumps(policy, indent=2) + '\n')
    print('USB1121_CURRENT_GPU_POLICY_READY' if a.finalize else 'USB1121_CURRENT_GPU_REFERENCE_ONLY_NO_DEVICE_IO')


if __name__ == '__main__':
    main()
