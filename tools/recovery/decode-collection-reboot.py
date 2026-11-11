#!/usr/bin/env python3
"""Read-only decoder for independent 216-byte collection proof schema1."""
import argparse,json,struct
from pathlib import Path
FIELDS='magic version bytes run_id service_pid result diagnostic_complete finalize_error log_flush_error log_close_error readonly_uncertain cancelled client_count io_error'.split()
CLIENT='observed phase client_id pid created wait_status exit_valid exit_code result io_error'.split()
def decode(raw):
 if len(raw)!=216:raise ValueError('collection proof must be exactly216 bytes')
 p=dict(zip(FIELDS,struct.unpack_from('<14I',raw)))
 if (p['magic'],p['version'],p['bytes'])!=(0x31524341,1,216):raise ValueError('unknown collection proof magic/version/size')
 p['policy']=raw[56:120].decode('ascii')
 if len(p['policy'])!=64 or any(c not in '0123456789abcdef' for c in p['policy']):raise ValueError('invalid policy encoding')
 p['clients']=[dict(zip(CLIENT,struct.unpack_from('<10I',raw,120+i*40))) for i in range(2)]
 if any(struct.unpack_from('<4I',raw,200)):raise ValueError('nonzero reserved words')
 p.update(schema='MT8167_COLLECTION_REBOOT_PROOF_V1',hardware_success=False,shutdown_execution_proven=False)
 return p
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('proof',type=Path);a=ap.parse_args();print(json.dumps(decode(a.proof.read_bytes()),indent=2))
