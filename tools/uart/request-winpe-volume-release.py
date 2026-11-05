#!/usr/bin/env python3
"""Request the fixed WinPE volume release after its current-boot UART READY."""
import argparse, os, stat, time
from pathlib import Path
READY=b'A04VOL_LISTENER_READY';OK=b'A04VOL_RELEASED'
FAIL={b'A04VOL_LAUNCH_FAILED',b'A04VOL_RELEASE_UNKNOWN',b'A04VOL_RELEASE_FAILED'}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uart-log',type=Path,required=True);p.add_argument('--offset',type=int,required=True,help='UART log byte position recorded before the diagnostic boot')
    p.add_argument('--uart',type=Path,default=Path('/dev/ttyUSB0'));p.add_argument('--timeout',type=int,default=1200)
    p.add_argument('--reset-after-release',action='store_true',help='request watchdog reset only after a complete success line')
    a=p.parse_args()
    if a.offset<0 or not 1<=a.timeout<=3600:raise SystemExit('invalid offset or timeout')
    info=a.uart_log.stat();dev=a.uart.stat()
    if not stat.S_ISREG(info.st_mode) or not stat.S_ISCHR(dev.st_mode) or a.offset>info.st_size:raise SystemExit('invalid log, offset or UART device')
    deadline=time.monotonic()+a.timeout;pending=b'';requested=False
    with a.uart_log.open('rb',buffering=0) as log:
        log.seek(a.offset)
        while time.monotonic()<deadline:
            now=a.uart_log.stat()
            if (now.st_dev,now.st_ino)!=(info.st_dev,info.st_ino) or now.st_size<log.tell():raise SystemExit('UART log replaced or truncated')
            data=log.read(65536)
            if not data:time.sleep(.25);continue
            pending+=data
            while b'\n' in pending:
                line,pending=pending.split(b'\n',1);line=line.rstrip(b'\r')
                if line==READY and not requested:
                    fd=os.open(a.uart,os.O_WRONLY|os.O_NOCTTY)
                    try:
                        if os.write(fd,b'\nA04BR3!VOL\n')!=12:raise OSError('short UART write')
                    finally:os.close(fd)
                    requested=True;print('volume request sent',flush=True)
                elif requested and line in FAIL:raise SystemExit('volume release failed or unknown: '+line.decode())
                elif requested and line==OK:
                    print('guarded volume release acknowledged',flush=True)
                    if a.reset_after_release:
                        fd=os.open(a.uart,os.O_WRONLY|os.O_NOCTTY)
                        try:
                            if os.write(fd,b'\nA04BR3!RST\n')!=12:raise OSError('short UART write')
                        finally:os.close(fd)
                        print('reset requested',flush=True)
                    return
            if len(pending)>4096:raise SystemExit('UART line exceeded limit; acknowledgement unknown')
    raise SystemExit('timeout; no verified volume release')
if __name__=='__main__':main()
