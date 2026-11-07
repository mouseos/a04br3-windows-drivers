#!/usr/bin/env python3
"""Drive the real host coordinator with a pseudo-UART and a growing log."""
import os, pathlib, pty, select, subprocess, tempfile, time, tty
root=pathlib.Path(__file__).resolve().parents[1]
tool=root/'tools/uart/request-winpe-volume-release.py'
def case(success):
    master,slave=pty.openpty();tty.setraw(slave)
    try:
        with tempfile.TemporaryDirectory() as d:
            log=pathlib.Path(d)/'uart.log';log.write_bytes(b'old\n');offset=log.stat().st_size
            proc=subprocess.Popen(['python3',str(tool),'--uart-log',str(log),'--offset',str(offset),'--uart',os.ttyname(slave),'--timeout','5','--reset-after-release'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            time.sleep(.15)
            with log.open('ab',buffering=0) as f:
                f.write(b'A04VOL_LISTENER_READY\nA04VOL_RELEASED\n');assert select.select([master],[],[],3)[0]
                assert os.read(master,100)==b'\nA04BR3!VOL\n'
                time.sleep(.3); assert not select.select([master],[],[],.1)[0]
                f.write(b'A04VOL_RELEASED\n' if success else b'A04VOL_RELEASE_FAILED\n')
                if success:
                    assert select.select([master],[],[],3)[0];assert os.read(master,100)==b'\nA04BR3!RST\n'
                out,err=proc.communicate(timeout=5);assert (proc.returncode==0)==success,(out,err)
                if not success:assert not select.select([master],[],[],.1)[0]
    finally:os.close(master);os.close(slave)
case(True);case(False)
print('host waits for ready and only resets after volume release success')
