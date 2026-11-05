#!/usr/bin/env python3
"""Ensure the old entry point cannot reach its former DISM path."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
lines = (root / 'tools/winpe/startnet-upgrade-toprgu.cmd').read_text().splitlines()
commands = [s.strip().lower() for s in lines if s.strip() and not s.strip().lower().startswith('rem ')]
assert commands == ['@echo off', 'echo toprgu_upgrade_disabled_no_safe_transaction', 'exit /b 2'], commands
print('legacy TOPRGU upgrade rejects without disk or device access')
