#!/usr/bin/env python3
"""Decode captured A04AUDKS UART records; never opens a serial/device handle."""
import argparse
import json
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[4]
RECORD = re.compile(r'A04AUDKS r=([0-9A-Fa-f]{8}) ([a-z0-9-]+) a=([0-9A-Fa-f]{8}) b=([0-9A-Fa-f]{8}) c=([0-9A-Fa-f]{8})')
FIELDS = {'state', 'transport', 'valid', 'dma', 'cur', 'irq', 'afe', 'lease', 'flags',
          'codec', 'analog0', 'analog1', 'speaker', 'trim', 'rails'}
STAGES = {1: 'created', 2: 'digital', 3: 'voice', 4: 'dma', 5: 'unmute',
          6: 'running', 7: 'stop', 8: 'stopped_pinned', 9: 'revoked'}


def summarize(run):
    frames = run['snapshots']
    samples = run['samples']
    running = next((f for f in frames if f['phase'] == 1), {})
    stopped = next((f for f in reversed(frames) if f['phase'] == 3), {})
    valid_positions, offsets, faults = [], [], []
    dma = running.get('dma')
    if dma:
        base, end, size = dma
        for sample in samples:
            if not all(k in sample for k in ('header', 'cur', 'codec', 'valid')):
                continue
            _, _, valid = sample['header']
            cur, offset, _ = sample['cur']
            if valid & 14 == 14 and base <= cur <= end and offset == cur - base and offset < size and offset % 4 == 0:
                valid_positions.append(cur)
                offsets.append(offset)
            faults.append(sample['codec'][2])
    changes = sum(a != b for a, b in zip(offsets, offsets[1:]))
    codec_enabled = False
    if FIELDS <= running.keys():
        group = running['valid'][1]
        analog = running['analog0']
        spk = running['speaker']
        trim = running['trim']
        rails = running['rails']
        codec_enabled = bool(group & 61 == 61 and analog[1] & (1 << 15) and analog[2] & (1 << 8)
                             and spk[0] & 1 and spk[1] & 0xF00 == 0x100 and not trim[2] & (1 << 14)
                             and rails[0] & 0xC000 == 0xC000 and rails[1] & 0xC000 == 0xC000)
    stopped_muted = bool(FIELDS <= stopped.keys() and stopped['valid'][0] & 2 and stopped['valid'][1] & 8
                         and not stopped['afe'][0] & 2 and not stopped['speaker'][1] & 0xF00)
    retained = bool(stopped and stopped.get('flags', (0, 0, 0))[0:2] == (1, 1)
                    and stopped.get('lease', (0, 0, 0))[1:3] == (1, 1))
    complete = bool(run.get('begin') and run.get('end'))
    measurement_complete = (len(samples) == 8
                            and [sample.get('header', (-1,))[0] for sample in samples] == list(range(8))
                            and [frame['phase'] for frame in frames] == [0, 1, 2, 3]
                            and all(FIELDS <= frame.keys() for frame in frames)
                            and all({'header', 'cur', 'codec', 'valid'} <= sample.keys() for sample in samples))
    state_requests_ok = all(any(item['tag'] == tag and item['values'][0] == 0 for item in run['records'])
                            for tag in ('run', 'pause-after', 'stop'))
    end = run.get('end')
    queue_failed = bool(end and end[1]) or any(item['tag'] == 'uart-queue-error' for item in run['records'])
    observed = bool(complete and measurement_complete and state_requests_ok and end[0] == 0 and not queue_failed and changes and codec_enabled
                    and stopped_muted and retained and not any(faults))
    states = [f['state'][2] for f in frames if f.get('state', (0, 0, 0))[2]]
    return {
        'run_id': run['run_id'], 'capture_ordinal': run['ordinal'],
        'result': 'digital_playback_observed' if observed else 'failure' if end and (end[0] or queue_failed) else 'incomplete_or_insufficient',
        'end_received': bool(end), 'client_exit_code': end[0] if end else None,
        'uart_queue_failed_before_end': bool(end[1]) if end else None,
        'valid_cur_samples': len(valid_positions), 'adjacent_cur_changes': changes,
        'dma_progress_observed': bool(changes), 'codec_enabled_snapshot': codec_enabled,
        'stop_and_mute_observed': stopped_muted, 'resources_retained': retained,
        'kernel_failure_stage': STAGES.get(states[-1], states[-1]) if states else None,
        'audible_verified': False,
        'note': 'CUR differences are sampled offsets, not an elapsed-frame count or proof of audible sound. Missing valid bits are unknown.',
        'records': run['records'], 'snapshots': frames, 'samples': samples,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    capture = args.capture.resolve(strict=True)
    if not capture.is_relative_to(PROJECT):
        parser.error('Capture must be within TAB-A04-BR3')
    if args.output and not args.output.resolve().is_relative_to(PROJECT):
        parser.error('Output must be within TAB-A04-BR3')
    sessions, latest = [], {}
    for number, line in enumerate(capture.read_text(errors='replace').splitlines(), 1):
        match = RECORD.search(line)
        if not match:
            continue
        ident, tag, *values = match.groups()
        value = tuple(int(x, 16) for x in values)
        if ident not in latest or tag == 'begin':
            run = {'run_id': ident, 'ordinal': len(sessions) + 1, 'records': [], 'snapshots': [], 'samples': []}
            sessions.append(run)
            latest[ident] = run
        run = latest[ident]
        run['records'].append({'line': number, 'tag': tag, 'values': value})
        if tag in ('begin', 'end'):
            run[tag] = value
        elif tag == 'snapshot':
            run['snapshots'].append({'phase': value[0], 'sequence': value[1], 'tick': value[2]})
        elif tag in FIELDS and run['snapshots']:
            run['snapshots'][-1][tag] = value
        elif tag == 'sample':
            run['samples'].append({'header': value})
        elif tag in ('sample-cur', 'sample-codec', 'sample-valid') and run['samples']:
            run['samples'][-1][tag.removeprefix('sample-')] = value
    result = {'capture': str(capture), 'audible_verified': False, 'runs': [summarize(run) for run in sessions]}
    data = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if args.output:
        args.output.write_text(data)
    else:
        print(data, end='')


if __name__ == '__main__':
    main()
