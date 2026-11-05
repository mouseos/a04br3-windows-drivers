#!/usr/bin/env python3
"""Reject board and development labels in INF device descriptions."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
errors = []
for inf in sorted((root / "drivers").rglob("*.inf")):
    text = inf.read_text(encoding="utf-8-sig")
    match = re.search(r"(?ims)^\[Strings\]\s*$(.*?)(?=^\[|\Z)", text)
    if not match:
        errors.append(f"{inf.relative_to(root)}: missing Strings section")
        continue
    descriptions = re.findall(r'^\s*(?:Device|DeviceDescription)\s*=\s*"([^"]+)"', match.group(1), re.I | re.M)
    if not descriptions:
        errors.append(f"{inf.relative_to(root)}: missing device description")
    for value in descriptions:
        if re.search(r"a04[-_ ]?br3|experimental|diagnostic", value, re.I):
            errors.append(f"{inf.relative_to(root)}: forbidden device description: {value}")
        if not re.search(r"\b(?:MT|BU|LC|MMA|GE)\d+[A-Z0-9]*\b", value, re.I):
            errors.append(f"{inf.relative_to(root)}: no hardware model in device description: {value}")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("INF device descriptions contain hardware models and no development labels")
