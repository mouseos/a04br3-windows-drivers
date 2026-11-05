#!/usr/bin/env python3
"""Analyze one explicit UART capture suffix; never access or control a device."""
import argparse, hashlib, json, re
from pathlib import Path

def analyze(data):
    ebs = re.search(rb'A04TRACE ExitBootServices (?:event signaled|return status=00000000)',data)
    ownership = re.search(rb'A04RGU CPU1 ownership acquired CPUs>=4\r?\n',data)
    resets = list(re.finditer(rb'BOOT_REASON:\s*(\d+)',data))
    kicks = list(re.finditer(rb'A04RGU timer kick\r?\n',data))
    owner = ownership.start() if ownership else None
    intervening_reset = bool(ebs and owner is not None and any(ebs.start()<m.start()<owner for m in resets))
    after_reset = bool(owner is not None and any(m.start()>owner for m in resets))
    kick_count = sum(m.start()>owner for m in kicks) if owner is not None else 0
    success = bool(ebs and ownership and ebs.start()<owner and kick_count>=2 and not intervening_reset and not after_reset)
    return {'exit_boot_services_marker':bool(ebs),'cpu1_ownership_marker':bool(ownership),
            'subsequent_kick_markers':kick_count,'boot_reason_after_ownership':after_reset,
            'boot_reason_between_ebs_and_ownership':intervening_reset,
            'last_boot_reason_before_ownership':next((int(m.group(1)) for m in reversed(resets) if owner is None or m.start()<owner),None),
            'uart_handoff_observed':success,
            'limitations':['no proof of driver identity, signature, OS volume, elapsed duration or long-term stability',
                           'ExitBootServices event signal alone does not prove return success',
                           'capture must start before the selected boot; incomplete UART can hide resets']}

def main():
    p=argparse.ArgumentParser();p.add_argument('uart_log',type=Path);p.add_argument('--offset',required=True,type=int)
    a=p.parse_args(); data=a.uart_log.read_bytes()
    if a.offset<0 or a.offset>=len(data): p.error('offset must be within a non-empty UART capture')
    data=data[a.offset:]; result=analyze(data)
    result.update(offset=a.offset,capture_bytes=len(data),capture_sha256=hashlib.sha256(data).hexdigest())
    print(json.dumps(result,indent=2)); return 0 if result['uart_handoff_observed'] else 1
if __name__=='__main__':raise SystemExit(main())
