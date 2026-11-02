#!/usr/bin/env python3
"""Capture one boot, preserve the first crash, and request UART reset on failure."""
import argparse
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--log', type=Path, required=True, help='既存のUART収集ログ')
p.add_argument('--offset', type=int, required=True, help='書込み前に記録したバイト位置')
p.add_argument('--output', type=Path, required=True, help='今回の起動ログ')
p.add_argument('--snapshot-prefix', type=Path, required=True)
p.add_argument('--uart', default='/dev/ttyUSB0')
p.add_argument('--timeout', type=int, default=300)
p.add_argument('--snapshot-after', type=int, default=120, help='EBS後の画面取得までの秒数')
a = p.parse_args()
if a.offset < 0 or a.timeout < 1 or a.snapshot_after < 1 or a.output.exists():
    raise SystemExit('オフセット・時間または出力先が不正です')
capture = Path(__file__).with_name('capture-framebuffer.py')
deadline = time.monotonic() + a.timeout
ebs_at = None
reported = set()
def reset():
    subprocess.run(['stty', '-F', a.uart, '921600', 'cs8', '-cstopb', '-parenb',
                    'raw', '-echo', 'clocal'], check=True)
    with open(a.uart, 'wb', buffering=0) as uart:
        uart.write(b'\nA04BR3!RST\n')

markers = (b'A04BR3 recovery: EBS', b'A04WIN DriverEntry entered',
           b'A04WIN FindAdapter entered', b'A04WIN guarded Windows range=',
           b'A04WIN SCSI GPT signature=')
with a.log.open('rb') as source, a.output.open('xb') as out:
    source.seek(a.offset)
    tail = b''
    while time.monotonic() < deadline:
        data = source.read()
        if data:
            out.write(data)
            out.flush()
            tail = (tail + data)[-524288:]
            for marker in markers:
                if marker in tail and marker not in reported:
                    print(marker.decode(), flush=True)
                    reported.add(marker)
                    if marker == markers[0]:
                        ebs_at = time.monotonic()
            if b'A04WIN BUGCHECK' in tail or b'A04WIN write range rejected' in tail:
                print('停止または書込み範囲検証失敗を検出、UARTでリセットします', flush=True)
                reset()
                raise SystemExit(2)
        if ebs_at is not None and time.monotonic() - ebs_at >= a.snapshot_after:
            print('フレームバッファを取得します', flush=True)
            for layer in ('0', '3'):
                output = f'{a.snapshot_prefix}-layer{layer}.pbm'
                subprocess.run(['python3', str(capture), '--log', str(a.log), '--uart', a.uart,
                                '--output', output, '--layer', layer, '--timeout', '15'], check=False)
            # The collector stayed active during framebuffer capture; preserve its new bytes too.
            out.write(source.read())
            out.flush()
            raise SystemExit(0)
        time.sleep(1)
print('観測期限までに画面取得段階へ到達せず、UARTでリセットします', flush=True)
reset()
raise SystemExit(1)
