#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Decode retained-ram.bin: the 512 bytes of .noinit at 0x01C7C000 that fm1-recover saves from a unit in UBOOT.

  tools/fm1_retained_ram.py retained-ram.bin [more.bin ...]

Boot-protected builds reserve the first 32 bytes for the stub mailbox; the
application layout then starts at 0x01C7C020. Earlier builds start at 0x01C7C000.
Layout (app.ld NOINIT, link order): fm1_crash, melodee_dbg, bootguard, then fm1_core1_bar (dual core only),
oc_guard (1.3.0 .. 1.4.0-rc.2 only) and panel. The loader leaves this RAM alone (loader.ld), so a unit that entered UBOOT
through the boot guard still holds what its last boots left here.
"""
import struct
import sys

CRASH = ('magic', 'count', 'vec', 'pc', 'rets', 'emu', 'dbg', 'sp', 'psr', 'icfg', 'uptime_ms',
         'etm0', 'etm1', 'etm2', 'etm3', 'early')
DBG = ('magic', 'halves', 'max_us', 'nested', 'in_audio', 'late', 'timer_irqs', 'ui_frames', 'last_us', 'cpu_q8',
       'boots', 'stage', 'page', 'home', 'prev_stage', 'prev_page', 'prev_home', 'prev_rst', 'prev_frames')
RST = ('power-on', 'VDDIO low', 'watchdog', 'VCM', 'long press', '1.2 V', 'soft (P33)')
MAGIC = {'crash': 0x43525348, 'dbg': 0x44424731, 'bootguard': 0x42475244, 'bar': 0x52414231, 'oc': 0x4F43474B,
         'panel': 0x50414E35}


def words(b, off, names):
    return dict(zip(names, struct.unpack_from('<%dI' % len(names), b, off)))


def rst(v):
    return ', '.join(n for i, n in enumerate(RST) if v >> i & 1) or 'none'


def decode(b):
    if len(b) < 0xB0:
        raise SystemExit('expected 512 bytes from 0x01C7C000')
    out = []
    info = struct.unpack_from('<I', b, 4)[0]
    if info & 0xFFFF0000 == 0x53420000:
        if len(b) < 0xD0:
            raise SystemExit('boot-stub dump is truncated')
        _, _, ticks, detail = struct.unpack_from('<4I', b)
        decisions = {1: 'application', 2: 'update loader', 3: 'ROM UBOOT'}
        out.append('boot stub  v%d, %s, %.1f ms; marker %d, loader %d, prior rescues %d' %
                   ((info >> 8) & 255, decisions.get(info & 255, 'unknown'), ticks / 24000,
                    detail & 255, (detail >> 8) & 255, (detail >> 16) & 255))
        b = b[32:]
    c = words(b, 0x00, CRASH)
    if c['magic'] == MAGIC['crash']:
        out.append('crash      count %(count)d  vec %(vec)d  pc %(pc)08x  rets %(rets)08x  dbg %(dbg)08x  '
                   'emu %(emu)08x  %(uptime_ms)d ms after boot  early %(early)d' % c)
        out.append('           etm %08x %08x %08x %08x' % (c['etm0'], c['etm1'], c['etm2'], c['etm3']))
    else:
        out.append('crash      no record (no CPU exception since power-on)')
    d = words(b, 0x40, DBG)
    if d['magic'] == MAGIC['dbg']:
        out.append('boots      %(boots)d past the splash; the last one: reset by ' % d + rst(d['prev_rst']) +
                   ', main loop stage %(stage)d, %(ui_frames)d UI frames' % d)
    else:
        out.append('boots      no record (no boot reached the splash since power-on)')
    g = words(b, 0x8C, ('magic', 'failed', 'pending'))
    out.append('bootguard  ' + ('failed %(failed)d pending %(pending)d' % g if g['magic'] == MAGIC['bootguard']
                                else 'no record'))
    # after bootguard: fm1_core1_bar (dual core only), then oc_guard (1.3.0 .. 1.4.0-rc.2 only), then panel ("PAN5").
    # The bar's magic is written only once CPU1 was barred (power-on garbage before), so the layout follows the
    # magics that are always there: oc_guard's, else the panel's
    word = lambda off: struct.unpack_from('<I', b, off)[0]
    if word(0x98) == MAGIC['oc']:
        dual, oc_at = False, 0x98
    elif word(0xA0) == MAGIC['oc']:
        dual, oc_at = True, 0xA0
    else:
        dual, oc_at = word(0x98) != MAGIC['panel'], None
    if dual:
        bar = words(b, 0x98, ('magic', 'barred'))
        out.append('core1 bar  ' + ('barred %(barred)d' % bar if bar['magic'] == MAGIC['bar'] else 'not set since power-on'))
    if oc_at is None:
        out.append('oc_guard   none (%s layout: 1.3.4 / 1.4 and later, or before 1.3.0)' % ('dual-core' if dual else 'single-core'))
    else:
        out.append('oc_guard   live %d (%s layout, 1.3.x)' % (words(b, oc_at, ('magic', 'live'))['live'],
                                                            'dual-core' if dual else 'single-core'))
    return '\n'.join(out)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for path in sys.argv[1:]:
        print('==', path)
        with open(path, 'rb') as f:
            print(decode(f.read()))
