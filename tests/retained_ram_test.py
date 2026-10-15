#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Recovery diagnostics must read old dumps and the relocated application state."""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import fm1_retained_ram as R

legacy = bytearray(512)
struct.pack_into('<II', legacy, 0, R.MAGIC['crash'], 3)
struct.pack_into('<I', legacy, 0x40, R.MAGIC['dbg'])
struct.pack_into('<3I', legacy, 0x8C, R.MAGIC['bootguard'], 2, 1)
for decision, text in ((1, 'application'), (2, 'update loader'), (3, 'ROM UBOOT')):
    protected = bytearray(512)
    protected[32:] = legacy[:480]
    struct.pack_into('<4I', protected, 0, 0, 0x53420300 | decision, 1440000, 5 << 8)
    result = R.decode(protected)
    assert result.split('\n', 1)[1] == R.decode(legacy)
    assert 'boot stub  v3, %s, 60.0 ms' % text in result
try:
    R.decode(protected[:180])
    raise AssertionError('truncated dump accepted')
except SystemExit:
    pass
print('retained RAM: legacy/boot-stub layouts, rescue decisions and truncation passed')
