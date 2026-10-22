#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Theo Dainis (melodee-tess)
"""Put Tessera's demo projects (demos/*.tsp) into a backup of your own FM-1, to restore on the backup page.

    python3 tools/demo_backup.py MY-BACKUP.json OUT.json                  the three demos into slots B, C and D
    python3 tools/demo_backup.py MY-BACKUP.json OUT.json --slots D        only the first demo, into slot D
    python3 tools/demo_backup.py MY-BACKUP.json OUT.json --slots C,D demos/mosaic.tsp demos/koto-garden.tsp

MY-BACKUP.json is a backup the installer's backup page saved from your FM-1 (Tessera 2.0 or later: projects in the
TSP1 format). OUT.json is the same backup with the chosen slots holding the demos; everything else (the music playing,
the settings, your other slots and presets) is byte for byte as it was. Restoring it on the backup page writes the
whole backup, so save MY-BACKUP.json somewhere safe first: it is your way back. Each slot replaced is listed with the
name of the project it held."""
import argparse
import base64
import json
import pathlib
import struct
import sys
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEMOS = [ROOT / "demos" / n for n in ("still-tide.tsp", "mosaic.tsp", "koto-garden.tsp")]
SLOT_ID = {"A": 2, "B": 3, "C": 4, "D": 5}      # the backup's objects 2..5: project slots A..D (editor_backup.c)
TSP_MAGIC = b"TSP1"
TSP_MAX = 32768                                  # project_tsp.c TSP_MAX: the largest record a slot takes
TSP_NAME = 10                                    # the NAME part's id


def fnv1a(b):                                    # project.c proj_hash: FNV-1a over the record before its checksum
    h = 2166136261
    for x in b:
        h = ((h ^ x) * 16777619) & 0xFFFFFFFF
    return h


def tsp_name(rec):
    """a TSP1 record's NAME, or None when rec is not a whole, valid-looking record"""
    if len(rec) < 16 or rec[:4] != TSP_MAGIC or struct.unpack_from("<I", rec, 4)[0] != len(rec) or len(rec) > TSP_MAX:
        return None
    if struct.unpack_from("<I", rec, len(rec) - 4)[0] != fnv1a(rec[:-4]):
        return None
    pos, parts = 12, struct.unpack_from("<H", rec, 8)[0]
    for _ in range(parts):
        pid, plen = rec[pos], struct.unpack_from("<H", rec, pos + 2)[0]
        if pid == TSP_NAME:
            return rec[pos + 4:pos + 4 + plen].split(b"\0")[0].decode("ascii", "replace")
        pos += 4 + plen
    return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("backup", help="a backup of your FM-1 from the backup page (.json)")
    ap.add_argument("out", help="the backup to write, with the demos in it")
    ap.add_argument("--slots", default="B,C,D", help="the slots to put them in, in order (default B,C,D)")
    ap.add_argument("demos", nargs="*", type=pathlib.Path, help="the demo records (default: demos/*.tsp)")
    a = ap.parse_args()
    slots = [s.strip().upper() for s in a.slots.split(",") if s.strip()]
    demos = a.demos or DEMOS[:len(slots)]
    if any(s not in SLOT_ID for s in slots) or len(set(slots)) != len(slots):
        sys.exit("--slots: A, B, C or D, each once")
    if len(demos) != len(slots):
        sys.exit(f"{len(slots)} slot(s) for {len(demos)} demo(s)")
    if pathlib.Path(a.out).resolve() == pathlib.Path(a.backup).resolve():
        sys.exit("write OUT.json beside your backup, not over it")

    backup = json.loads(pathlib.Path(a.backup).read_text())
    if backup.get("format") != "felucca-backup" or backup.get("version") != 1 or not isinstance(backup.get("objects"), list):
        sys.exit(f"{a.backup}: not a backup from the backup page")
    objects = {o.get("id"): o for o in backup["objects"]}
    if len(backup["objects"]) != 30 or sorted(objects) != list(range(30)):
        sys.exit(f"{a.backup}: not a backup of Tessera (its own project format has 30 objects)")
    for o in backup["objects"]:
        data = base64.b64decode(o["data"]) if o["size"] else b""
        if len(data) != o["size"] or zlib.crc32(data) != o["crc"]:
            sys.exit(f"{a.backup}: object {o['id']} is damaged (size or checksum): take a new backup")
    music = base64.b64decode(objects[0]["data"])
    if tsp_name(music) is None:
        sys.exit(f"{a.backup}: its projects are not TSP1 records: back up again with Tessera 2.0 or later")

    for slot, path in zip(slots, demos):
        rec = path.read_bytes()
        name = tsp_name(rec)
        if name is None:
            sys.exit(f"{path}: not a whole TSP1 project record")
        o = objects[SLOT_ID[slot]]
        old = tsp_name(base64.b64decode(o["data"])) if o["size"] else None
        was = "empty" if not o["size"] else f'"{old}"' if old else "a project with no name" if old == "" else "an older project"
        o.update(size=len(rec), crc=zlib.crc32(rec), data=base64.b64encode(rec).decode("ascii"))
        print(f"slot {slot}: {name} (was {was})")
    pathlib.Path(a.out).write_text(json.dumps(backup, separators=(",", ":")))   # (as the page writes it)
    print(f"wrote {a.out}: restore it on the backup page; {a.backup} is your way back")


if __name__ == "__main__":
    main()
