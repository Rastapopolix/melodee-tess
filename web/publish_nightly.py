#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Update an existing Pages checkout with the next nightlies (dual and single core); preserve its release/editor
and the synth-only firmware."""
import argparse
from make_site import main, release_packages


def publish(out, dual, single, version, commit, built_at):
    release, packages = release_packages(out)
    main(packages["dual"], packages["single"], release, out,
         nightly={"pkg": {"dual": dual, "single": single}, "version": version,
                  "commit": commit, "builtAt": built_at}, installer_only=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out")
    parser.add_argument("--dual", required=True)
    parser.add_argument("--single", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--built-at", required=True)
    args = parser.parse_args()
    publish(args.out, args.dual, args.single, args.version, args.commit, args.built_at)
