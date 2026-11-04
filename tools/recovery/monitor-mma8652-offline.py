#!/usr/bin/env python3
"""Two exact current-image completion QR frames; never reset a device."""
import argparse
from pathlib import Path
import subprocess
import sys
import time
import cv2
import numpy as np
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'winpe'))
from mma8652_signal import payload

class Gate:
    def __init__(self, token):
        self.expected = payload(token)
        self.completed = 0
    def observe(self, decoded):
        self.completed = self.completed + 1 if decoded == self.expected else 0
        return self.completed >= 2

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--uart', required=True)
    p.add_argument('--uart-log', type=Path, required=True)
    p.add_argument('--boot-offset', type=int, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--token', required=True)
    p.add_argument('--timeout', type=float, default=1800)
    a = p.parse_args()
    gate = Gate(a.token)
    if a.timeout <= 0 or a.boot_offset < 0 or not a.output.is_dir(): return 2
    initial = a.uart_log.stat()
    last_size = initial.st_size
    if last_size < a.boot_offset: return 2
    first_boots = None
    deadline = time.monotonic() + a.timeout
    def read_log():
        nonlocal last_size
        st = a.uart_log.stat()
        if (st.st_dev, st.st_ino) != (initial.st_dev, initial.st_ino) or st.st_size < last_size:
            raise ValueError('UART log replaced or truncated')
        last_size = st.st_size
        with a.uart_log.open('rb') as log:
            log.seek(a.boot_offset)
            return log.read()
    for index in range(1000000):
        if time.monotonic() >= deadline:
            print('MMA_MONITOR_TIMEOUT_NO_RESTORE', flush=True); return 3
        try:
            before = read_log()
            if first_boots is not None and before.count(b'BOOT_REASON') != first_boots:
                raise ValueError('reboot detected')
            frame = a.output / f'frame-{index:04}.pbm'
            result = subprocess.run(['python3', str(a.capture), '--log', str(a.uart_log),
                '--uart', a.uart, '--output', str(frame), '--timeout', '45'],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=50)
            after = read_log()
            if after.count(b'BOOT_REASON') != before.count(b'BOOT_REASON'):
                raise ValueError('reboot during capture')
            decoded = ''
            if result.returncode == 0 and b'A04ACC UEFI I2C1 setup=Success' in after:
                image = np.asarray(Image.open(frame).convert('L'))
                detector = cv2.QRCodeDetector()
                for turns in range(4):
                    decoded = detector.detectAndDecode(np.ascontiguousarray(np.rot90(image, turns)))[0]
                    if decoded == gate.expected: break
            good = decoded == gate.expected
            if good and first_boots is None: first_boots = after.count(b'BOOT_REASON')
            if gate.observe(decoded):
                if read_log().count(b'BOOT_REASON') != first_boots: raise ValueError('reboot detected')
                print('MMA_MONITOR_EXACT_COMPLETION_TWICE', flush=True); return 0
            if not good: first_boots = None
        except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
            print(f'MMA_MONITOR_INDETERMINATE_NO_RESTORE {exc}', flush=True); return 4
        time.sleep(2)
    return 3
if __name__ == '__main__': raise SystemExit(main())
