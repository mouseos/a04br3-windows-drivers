#!/usr/bin/env python3
"""One host run: compile fixed synthetic C fixture, emit real8715 framing, decode."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('vu', repo/'tools/recovery/decode-vidpn-vu.py')
vu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vu)


def main():
    out = Path(sys.argv[1]).resolve()
    out.mkdir(parents=True, exist_ok=False)
    executable = out/'framing-fixture'
    compile_args = ['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    str(repo/'tests/vidpn_vu_framing_fixture.c'), '-o', str(executable)]
    built = subprocess.run(compile_args, capture_output=True)
    (out/'compile.log').write_bytes(built.stdout+built.stderr)
    built.check_returncode()
    raw = subprocess.check_output([str(executable)])
    missing = subprocess.check_output([str(executable), '--missing'])
    (out/'fixed8715-full44.uart').write_bytes(raw)
    (out/'fixed8715-missing.uart').write_bytes(missing)
    decoded = vu.decode(raw, 51760, 1428)
    assert len(decoded['records']) == 15 and decoded['wire_lines'] == 100
    assert decoded['current_boot_unproven'] and not decoded['source_proven'] and not decoded['hardware_success']
    assert decoded['raw_epoch_FILETIME'] == 42 and decoded['raw_epoch_interrupttime'] == 99
    assert decoded['summary_words'][7] == 1
    assert decoded['declared_writer_source_sha1'] == '01000000'+'00'*16
    (out/'decoded-fixture.json').write_text(json.dumps(decoded, indent=2)+'\n')
    checks = ['real_C_formatter_full44', 'unproven_and_synthetic_identity_preserved']
    lines = raw.splitlines(keepends=True)
    noise = b'UART ascii noise\nPB 00000000 00000207 00000001\n'
    noisy = b'BOOT_REASON 0\n'+noise+noise.join(lines)+noise
    assert vu.decode(noisy, 51760, 1428)['wire_lines'] == 100
    assert vu.decode(raw.replace(b'\n', b'\r\n'), 51760, 1428)['transport_complete']
    checks += ['ASCII_noise_and_PB_ignored', 'CRLF']

    def reject(name, data, run=51760, pid=1428):
        try:
            vu.decode(data, run, pid)
        except vu.Rejected as error:
            checks.append(dict(case=name, rejected=str(error)))
        else:
            raise AssertionError('accepted '+name)

    reject('missing_chunk', b''.join(lines[:2]+lines[3:]))
    reject('duplicate_chunk', lines[0]+raw)
    reject('unknown_event', raw.replace(b'000003D0', b'000003D9', 1))
    reject('oversize_line', b'VU '+b'0'*100+b'\n'+raw)
    reject('partial_final_line', raw[:-1])
    reject('missing_END', b''.join(lines[:-1]))
    reject('wrong_chunk_count', raw.replace(b'00000006', b'00000005', 1))
    reject('not_integer', raw.replace(b'000003D0', b'000003DG', 1))
    reject('interleaved_boot', b''.join(lines[:10])+b'BOOT_REASON 4\n'+b''.join(lines[10:]))
    reject('boot_after_snapshot_is_old', raw+b'BOOT_REASON 4\n')
    reject('two_boot_streams', raw+b'BOOT_REASON 4\n'+raw)
    reject('second_stream_same_identity', raw+raw)
    reject('wrong_run', raw, run=51761)
    reject('wrong_PID', raw, pid=1429)
    reject('prefix_contamination', b'noise '+raw)
    reject('101st_VU_line', raw+lines[-1])
    reject('ERROR_not_snapshot_success', missing)
    # Change an END count while preserving exact valid chunk framing.
    bad = lines[-1].split(b' ')
    bad[-1] = b'0000024F\n'  # Actual total is592/0x250.
    reject('END_total_words_mismatch', b''.join(lines[:-1])+b' '.join(bad))
    # The final binding chunk contains reserved words25..31.
    modified = list(lines)
    fields = modified[5].split(b' ')
    fields[-1] = b'00000001\n'
    modified[5] = b' '.join(fields)
    reject('binding_reserved_nonzero', b''.join(modified))
    modified = list(lines)
    fields = modified[0].split(b' ')
    fields[8] = b'00000001'  # binding[3] must NEVER claim current boot.
    modified[0] = b' '.join(fields)
    reject('forged_current_boot_claim', b''.join(modified))
    modified = list(lines)
    starts = [i for i, line in enumerate(lines) if line.split(b' ')[1] == b'000003D2'
              and line.split(b' ')[3] == b'00000000']
    fields = modified[starts[1]].split(b' ')
    fields[5] = b'00000005'
    modified[starts[1]] = b' '.join(fields)
    reject('duplicate_canonical_record', b''.join(modified))
    result = dict(host_only=True, fixture_source='fixed8715 synthetic fixture; actual C formatter',
                  host_compile_count=1, driver_builds=0, device_operations=0,
                  checks=checks, passed=True)
    (out/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(passed=True, checks=len(checks), output=str(out))))


if __name__ == '__main__':
    main()
