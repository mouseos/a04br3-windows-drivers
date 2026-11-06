#!/usr/bin/env python3
"""Replace the provider in the verified direct-POST WinPE image."""
import argparse, hashlib, json, os, re, shutil, struct, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
WIM = 'cc824c145c6bf7da880a492155ac0fa3b529678a648ce7916c57f2a59975a8a7'
ESP = 'c574246676a94c11f032c4e67b15dc56ead574f406a92fd5e2b9f5dd18c239f9'
BCD = '65376227bc92e7342017fafb8de291db348ed6f70edd1a7e9fa1f1c05f111c0d'

def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--base', type=Path, required=True)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--provider-sha256', required=True)
    p.add_argument('--display-package', type=Path)
    p.add_argument('--display-sha256')
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--workload', type=Path)
    p.add_argument('--workload-sha256')
    a = p.parse_args()
    base, package, out = (v.resolve() for v in (a.base, a.package, a.out))
    if any(not v.is_relative_to(ROOT) for v in (base, package, out)) or out.exists():
        raise SystemExit('Use workspace inputs and a new output directory')
    assert sha(base/'volume.wim') == WIM and sha(base/'volume-esp.img') == ESP
    assert sha(package/'mt8167-ge8300.sys') == a.provider_sha256
    assert bool(a.display_package) == bool(a.display_sha256)
    display = a.display_package.resolve() if a.display_package else None
    if display:
        assert display.is_relative_to(ROOT)
        assert sha(display/'mt8167-display.sys') == a.display_sha256
        assert all((display/name).is_file() for name in ('mt8167-display.inf','mt8167-display.cat'))

    assert sha(package/'ge8300-mips.fw') == '5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1'
    out.mkdir(parents=True)
    work = out/'stage'; work.mkdir()
    tools = ROOT/'port/xnu/out/windows-rt-tools'
    env = os.environ.copy()
    env.update(TMPDIR=str(work), LD_LIBRARY_PATH=str(tools/'wimtools'),
               PYTHONPATH=str(tools/'python-fat'), PYTHONDONTWRITEBYTECODE='1')
    def run(argv, **kw):
        return subprocess.run(argv, env=env, check=True, **kw)
    def wim(*args, **kw):
        return run([str(tools/'wimtools/wimlib-imagex'), *map(str,args)], **kw)
    for path in ('/Windows/System32/config/SOFTWARE', '/Windows/System32/startnet.cmd'):
        wim('extract', base/'volume.wim', '1', path, '--dest-dir='+str(work), '--no-acls')
    software = work/'SOFTWARE'
    cert = (package/'gpu-test.cer').read_bytes()
    thumb = hashlib.sha1(cert).hexdigest().upper()
    blob = struct.pack('<III',3,1,20)+hashlib.sha1(cert).digest()+struct.pack('<III',32,1,len(cert))+cert
    for store in ('Root','TrustedPublisher'):
        key = '\\Microsoft\\SystemCertificates\\'+store+'\\Certificates'
        before = run(['hivexsh',str(software)], input='cd '+key+'\nls\n',text=True,capture_output=True).stdout
        if thumb not in before.upper():
            commands='cd '+key+'\nadd '+thumb+'\ncd '+thumb+'\nsetval 1\nBlob\nhex:3:'+blob.hex()+'\ncommit\nquit\n'
            run(['hivexsh','-w',str(software)],input=commands,text=True,capture_output=True)
        assert run(['hivexget',str(software),key+'\\'+thumb,'Blob'],capture_output=True).stdout == blob
    # The direct-POST base predates the audit sender. Import the fixed
    # sender from the pinned maintenance image instead of assuming it exists.
    maintenance = ROOT/'port/xnu/out/windows-rt-cpu-usb-audit-scope-v2-mapfix/maintenance-staged.wim'
    assert sha(maintenance) == '0df19de6c783068b24c466a1d7afaa900cd99a0aef24841a319bbd566cbbedf4'
    wim('extract', maintenance, '1', '/a04br3/uart-audit-stream.exe', '--dest-dir='+str(work), '--no-acls')
    start = work/'startnet.cmd'
    text = start.read_text().replace('\r','')
    text = re.sub(r'echo A04USB_OBSERVATION_START\n.*?(?= echo PnP and touch)', '', text, flags=re.S)
    text = '\n'.join(line for line in text.split('\n') if
                     'fourcpu-load.exe' not in line and not line.startswith('echo A04CPU_'))
    text = re.sub(r'echo A04AUDIT_WAIT[^\n]*\nping[^\n]*\ncall[^\n]*run-audit-once.cmd\n?', '', text)
    assert 'fourcpu-load.exe' not in text and 'usb-power-observe.cmd' not in text
    assert 'run-audit-once.cmd' not in text and 'display-shared\\mt8167-display.inf' in text
    # Load the actual display path directly; independent copy self-tests and
    # status programs must not gate the frame measurement. Preserve stage
    # stdout in bounded CRC-framed UART reports when no camera is attached.
    text = text.replace('%GPU%\\ge8300-pnp-status.exe\n', '').replace('%GPU%\\ge8300-status.exe\n', '')
    text = text.replace('%GPU%\\ge8300-vendor-copy-test.exe\nif errorlevel 1 goto gpu_failed\n', '')
    text = text.replace('drvload %GPU%\\mt8167-ge8300.inf\n',
        'drvload %GPU%\\mt8167-ge8300.inf > X:\\a04br3\\toprgu-audit-result.txt 2>&1\n'
        'set GPU_LOAD_RC=%errorlevel%\n'
        'X:\\a04br3\\uart-audit-stream.exe 9917000000000001\n'
        'if not "%GPU_LOAD_RC%"=="0" goto gpu_failed\n')
    text = text.replace('drvload X:\\a04br3\\display-shared\\mt8167-display.inf\n',
        'drvload X:\\a04br3\\display-shared\\mt8167-display.inf > X:\\a04br3\\toprgu-audit-result.txt 2>&1\n'
        'set DISPLAY_LOAD_RC=%errorlevel%\n'
        'X:\\a04br3\\uart-audit-stream.exe 9917000000000002\n'
        'if not "%DISPLAY_LOAD_RC%"=="0" goto gpu_failed\n')
    text = text.replace('echo GE8300_COPY_DRVLOAD_EXIT=%errorlevel%\nif errorlevel 1 goto gpu_failed',
        'echo GE8300_COPY_DRVLOAD_EXIT=%GPU_LOAD_RC%')
    text = text.replace('echo A04_SHARED_DISPLAY_DRVLOAD_EXIT=%errorlevel%\nif errorlevel 1 goto gpu_failed',
        'echo A04_SHARED_DISPLAY_DRVLOAD_EXIT=%DISPLAY_LOAD_RC%')
    assert 'uart-audit-stream.exe' in text
    # Full-resolution UART screenshots are slow; collect only after frame
    # completion with a separate image if needed, not during timing.

    if a.workload:
        assert a.workload_sha256 and a.workload.resolve().is_relative_to(ROOT)
        assert sha(a.workload) == a.workload_sha256
        text = text.replace('echo A04_SHARED_DIAGNOSTIC_AUTOSTART_WAIT',
            'echo A04_SHARED_DIAGNOSTIC_AUTOSTART_WAIT\n'
            'X:\\a04br3\\ge8300-gdi-frame.exe > X:\\a04br3\\toprgu-audit-result.txt 2>&1\n'
            'set GDI_WORKLOAD_RC=%errorlevel%\n'
            'echo GE8300_GDI_WORKLOAD_EXIT=%GDI_WORKLOAD_RC% >> X:\\a04br3\\toprgu-audit-result.txt\n'
            'X:\\a04br3\\uart-audit-stream.exe 9917000000000003')
    start.write_bytes(text.replace('\n','\r\n').encode())
    image = out/'volume.wim'; shutil.copyfile(base/'volume.wim',image)
    changes = [(software,'/Windows/System32/config/SOFTWARE'),(start,'/Windows/System32/startnet.cmd'), (work/'uart-audit-stream.exe','/a04br3/uart-audit-stream.exe')]
    changes += [(package/name,'/a04br3/ge8300-copy/'+name) for name in
                ('mt8167-ge8300.sys','mt8167-ge8300.inf','mt8167-ge8300.cat')]
    if display:
        changes += [(display/name,'/a04br3/display-shared/'+name) for name in
                    ('mt8167-display.sys','mt8167-display.inf','mt8167-display.cat')]
    if a.workload: changes.append((a.workload.resolve(),'/a04br3/ge8300-gdi-frame.exe'))
    wim('update',image,'1',input=''.join('add '+str(src)+' '+dst+'\n' for src,dst in changes),text=True)
    wim('verify',image)
    helper = ROOT/'windows-rt/a04br3-windows-ge8300-copy-safe-stop/tools/winpe/replace-esp-wim.py'
    run(['python3',str(helper),'--template',str(base/'volume-esp.img'),'--wim',str(image),
         '--output',str(out/'volume-esp.img'),'--template-bcd-sha256',BCD,'--template-wim-sha256',WIM])
    (out/'manifest.json').write_text(json.dumps({'base_wim_sha256':WIM,'provider_sha256':a.provider_sha256,
       'display_sha256':a.display_sha256,'workload_sha256':a.workload_sha256,'test_signer_sha1':thumb,'wim_sha256':sha(image),'esp_sha256':sha(out/'volume-esp.img'),
       'status':'built; device performance unverified'},indent=2)+'\n')

if __name__ == '__main__': main()
