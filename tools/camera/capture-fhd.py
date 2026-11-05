#!/usr/bin/env python3
"""Capture a full-HD frame from the A04BR3 observation camera."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import struct
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--device', default='/dev/video0')
p.add_argument('--focus', type=int, default=10, help='V4L2 absolute focus')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if not 0 <= a.focus <= 40 or a.output.exists():
    raise SystemExit('invalid focus or output already exists')
fd = os.open(a.device, os.O_RDWR)
try:
    # V4L2_CID_FOCUS_AUTO and V4L2_CID_FOCUS_ABSOLUTE; explicit manual focus
    # prevents the lens hunting while the tablet displays a dark frame.
    for control, value in ((0x009a090c, 0), (0x009a090a, a.focus)):
        fcntl.ioctl(fd, 0xc008561c, struct.pack('Ii', control, value))
finally:
    os.close(fd)
a.output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 'v4l2',
                '-video_size', '1920x1080', '-i', a.device, '-frames:v', '1',
                '-n', str(a.output)], check=True)
probe = subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v:0',
    '-show_entries', 'stream=width,height', '-of', 'json', str(a.output)])
streams = json.loads(probe)['streams']
if len(streams) != 1 or (streams[0]['width'], streams[0]['height']) != (1920, 1080):
    raise SystemExit('camera did not produce a full-HD frame')
print(f'Full-HD frame saved: {a.output}')
