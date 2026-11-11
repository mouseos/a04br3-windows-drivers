#!/usr/bin/env python3
"""Host-only regression: mock transports; never open a real device.

Usage: python3 -B test-bounded-recovery.py ROOT_LOCAL_SCRATCH_DIRECTORY
"""
import ast
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

source = Path(__file__).with_name('run-gpu368-fresh200.py')
spec = importlib.util.spec_from_file_location('engine', source)
engine = importlib.util.module_from_spec(spec)
spec.loader.exec_module(engine)
tree = ast.parse(source.read_text())
recovery = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == 'recovery')
body = ast.literal_eval(recovery.body[0].value.args[0].left)
results = {}
with tempfile.TemporaryDirectory(dir=Path(sys.argv[1]).resolve()) as temporary:
    scratch = Path(temporary)
    mock = scratch / 'transport'
    mock.write_text('''#!/usr/bin/python3
import os,sys,time
from pathlib import Path
name=Path(sys.argv[0]).name
args=sys.argv[1:]
if args[:1]==['-s']: args=args[2:]
with open(os.environ['CALLS'],'a') as f: f.write(name+' '+ ' '.join(args)+'\\n')
mode=os.environ['MODE']
if mode=='hang': time.sleep(300)
elif name=='fastboot':
 if args==['devices'] and mode!='absent': print('MOCKSERIAL fastboot')
 elif args==['getvar','product']: print('product: A04BR3')
elif args==['get-state']: print('recovery' if mode=='ready' else 'unknown')
elif args==['shell','id','-u']: print('0')
''')
    mock.chmod(0o755)
    for name in ('adb', 'fastboot'):
        (scratch / name).symlink_to(mock)
    reset = scratch / 'reset.sh'
    reset.write_text('exit 0\n')
    calls = scratch / 'calls'
    env = dict(os.environ, PATH=str(scratch)+':'+os.environ['PATH'], CALLS=str(calls))
    prefix = 'set -euo pipefail\nrt_target_serial=MOCKSERIAL\n'+engine.BOUNDED_DEVICE_SHELL
    def run(script, mode, limit=20):
        calls.write_text('')
        with (scratch / 'output').open('wb') as output:
            return engine.run_command_group(['bash', '-c', prefix+script, 'mock', 'unused', str(reset)],
                                            env=dict(env, MODE=mode), stdout=output,
                                            stderr=subprocess.STDOUT, timeout=limit)
    # Only the wait budget is accelerated; execute the actual recovery body.
    try:
        run(body.replace('SECONDS+90', 'SECONDS+0'), 'absent')
        raise AssertionError('missing fastboot accepted')
    except subprocess.CalledProcessError:
        assert 'FASTBOOT_NOT_FOUND_AFTER_RESET' in (scratch/'output').read_text()
        assert 'getvar' not in calls.read_text() and 'oem' not in calls.read_text()
        results['absent_fastboot_stops_before_getvar_oem'] = True
    run(body, 'ready')
    assert 'fastboot' not in calls.read_text()
    results['already_recovery_skips_reset_and_fastboot'] = True
    for command in ('fastboot getvar product', 'fastboot oem reboot-recovery', 'adb get-state'):
        started = time.monotonic()
        try:
            run(command, 'hang')
            raise AssertionError('hung transport accepted')
        except subprocess.CalledProcessError as error:
            assert error.returncode == 124
            assert time.monotonic()-started < 15
        results[command+'_bounded'] = True
    # A TERM-resistant grandchild must die, while an unrelated process survives.
    child_pid = scratch / 'child.pid'
    unrelated = subprocess.Popen(['sleep', '60'])
    try:
        command = ['bash', '-c', 'trap "" TERM; sleep 60 & echo $! > "$1"; wait', 'mock', str(child_pid)]
        try:
            engine.run_command_group(command, timeout=0.5)
            raise AssertionError('outer timeout missing')
        except subprocess.TimeoutExpired:
            pid = int(child_pid.read_text())
            for _ in range(20):
                stat = Path('/proc') / str(pid) / 'stat'
                if not stat.exists() or stat.read_text().split()[2] == 'Z': break
                time.sleep(0.05)
            else: raise AssertionError('grandchild survived')
            assert unrelated.poll() is None
        results['timeout_kills_only_own_process_group'] = True
    finally:
        unrelated.terminate()
        unrelated.wait()
print(json.dumps(results, indent=2))
