#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Add or update the synth-only firmware on an existing Pages checkout: its two packages (dual and single core, from
the synth-only branch: one track, every synth and the effects, no sequencer) and its editor (the branch's web/, at
webapp/synth-editor/). The installer offers it as an option; the release, the nightlies and the release editor stay.

  web/publish_synth.py PAGES --dual build/melodee-synth-nightly.fwsc --single build/melodee-synth-nightly-1core.fwsc \\
      --version synth-abcdef0 --commit <full SHA> --built-at 2026-10-12T08:00:00Z --web <synth-only checkout>/web
"""
import argparse
from make_site import main, release_packages


def publish(out, dual, single, version, commit, built_at, web):
    release, packages = release_packages(out)
    main(packages["dual"], packages["single"], release, out,
         synth={"pkg": {"dual": dual, "single": single}, "version": version, "commit": commit, "builtAt": built_at,
                "web": web}, installer_only=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out")
    parser.add_argument("--dual", required=True)
    parser.add_argument("--single", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--built-at", required=True)
    parser.add_argument("--web", required=True, help="the synth-only branch's web/ (its editor)")
    args = parser.parse_args()
    publish(args.out, args.dual, args.single, args.version, args.commit, args.built_at, args.web)
