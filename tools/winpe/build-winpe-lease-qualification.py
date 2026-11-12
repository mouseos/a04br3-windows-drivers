#!/usr/bin/env python3
"""通常Windowsの更新を要求せず、WinPEの期限リセットを試す固定イメージを作る。"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from winpe_lease_package import inject, load_atomic_helper, need, sha

BASE_WIM = '0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'
BASE_ESP = '22ac309921bd9ae405bd48e65d8cb8a70506c90f0c08e6edda4f0150fb4993ce'
BCD = '65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for n in ('workspace', 'base', 'tools', 'lease-manifest', 'atomic-manifest', 'out'):
        ap.add_argument('--' + n, type=Path, required=True)
    for n in ('lease-sha256', 'atomic-sha256'):
        ap.add_argument('--' + n, required=True)
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)
    repo = Path(__file__).resolve().parents[2]
    for name in ('base', 'tools', 'lease_manifest', 'atomic_manifest', 'out'):
        path = (root / getattr(a, name)).resolve()
        need(path.is_relative_to(root), '作業root外のpathは使用できません')
        setattr(a, name, path)
    need(repo.is_relative_to(root) and not a.out.exists(), '新しいroot内出力先が必要です')
    need(not subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain']),
         '固定したclean sourceが必要です')
    commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    need(sha(a.base / 'maintenance-staged.wim') == BASE_WIM
         and sha(a.base / 'maintenance-staged-esp.img') == BASE_ESP, '固定WinPE baseと異なります')
    _, helper = load_atomic_helper(root, a.atomic_manifest, a.atomic_sha256)
    token = sha(a.lease_manifest)[:16].upper()
    need(sha(a.lease_manifest) == a.lease_sha256, 'lease manifest SHA不一致')
    a.out.mkdir(parents=True)
    stage = a.out / 'stage'
    stage.mkdir()
    report = r'X:\a04br3\toprgu-audit-result.txt'
    script = '\n'.join(('@echo off', 'mountvol /N',
        '> "' + report + '" echo LEASE_QUALIFICATION_BEGIN',
        'wpeinit', '>> "' + report + '" echo WPEINIT_EXIT=%errorlevel%',
        'mountvol /N', '>> "' + report + '" echo AUTOMOUNT_DISABLE_EXIT=%errorlevel%',
        'X:\\a04br3\\atomic-stage-replace.exe --check-lease >> "' + report + '" 2>&1',
        '>> "' + report + '" echo LEASE_QUERY_EXIT=%errorlevel%',
        '>> "' + report + '" echo LEASE_EXPECTED_SECONDS=1800',
        '>> "' + report + '" echo NORMAL_WINDOWS_UPDATE_REQUESTED=0',
        'X:\\a04br3\\uart-audit-stream.exe ' + token,
        'rem ホストのUART要求ではなく、独立したカーネル期限リセットを待つ。',
        'exit /b 0', ''))
    # CMDのコメントもASCIIに限定し、WinPE codepageへの依存を避ける。
    script = script.replace('rem ホストのUART要求ではなく、独立したカーネル期限リセットを待つ。',
                            'rem Wait for the independent kernel lease reset.')
    startnet = stage / 'startnet.cmd'
    startnet.write_bytes(script.replace('\n', '\r\n').encode('ascii'))
    image = a.out / 'qualification.wim'
    shutil.copyfile(a.base / 'maintenance-staged.wim', image)
    env = os.environ.copy()
    env.update(TMPDIR=str(stage), LD_LIBRARY_PATH=str(a.tools / 'wimtools'),
               PYTHONPATH=str(a.tools / 'python-fat'), PYTHONDONTWRITEBYTECODE='1')
    wimtool = str(a.tools / 'wimtools/wimlib-imagex')
    commands = ('add "' + str(startnet) + '" /Windows/System32/startnet.cmd\n'
                + 'add "' + str(helper) + '" /a04br3/atomic-stage-replace.exe\n')
    subprocess.run([wimtool, 'update', str(image), '1'], input=commands, text=True, env=env, check=True)
    proof = inject(image, root, a.tools, a.out / 'winpe-lease-verified', env,
                   a.lease_manifest, a.lease_sha256)
    for i, (dest, original) in enumerate((('/Windows/System32/startnet.cmd', startnet),
                                        ('/a04br3/atomic-stage-replace.exe', helper))):
        check = a.out / ('verified-command-' + str(i))
        check.mkdir()
        subprocess.run([wimtool, 'extract', str(image), '1', dest, '--dest-dir=' + str(check)],
                       env=env, check=True)
        leaves = [p for p in check.rglob('*') if p.is_file()]
        need(len(leaves) == 1 and sha(leaves[0]) == sha(original), '資格確認コマンドの実byteが異なります')
    esp = a.out / 'qualification-esp.img'
    subprocess.run(['python3', '-B', str(Path(__file__).with_name('replace-esp-wim.py')),
                    '--template', str(a.base / 'maintenance-staged-esp.img'), '--wim', str(image),
                    '--output', str(esp), '--template-bcd-sha256', BCD,
                    '--template-wim-sha256', BASE_WIM], env=env, check=True)
    result = dict(schema='WINPE_UNATTENDED_LEASE_QUALIFICATION_V1',
                  status='host-built-not-hardware-verified', source_commit=commit,
                  winpe_lease=proof, atomic_manifest_sha256=a.atomic_sha256,
                  atomic_EXE_sha256=sha(helper), startnet_sha256=sha(startnet),
                  UART_token=token, wim_sha256=sha(image), esp_sha256=sha(esp),
                  offline_windows_update_requested=False, boot_write_required=False,
                  gpt_write_required=False, expected_reset_seconds=1800,
                  expected_reset_requires_no_UART_RX=True, hardware_verified=False)
    (a.out / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print('WINPE_LEASE_QUALIFICATION_IMAGE_READY_NOT_DEVICE_VERIFIED')


if __name__ == '__main__':
    main()
