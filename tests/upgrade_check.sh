#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Does a project saved by an older build load in this one? tests/upgrade_test.c's save side built from REF (default
# tessera-dev) in a temporary worktree, its load side from this tree. REF must build the host tests with Tessera
# (tessera-dev's d9a564b on): the 1.0 release (main) cannot, and saves exactly as tessera-dev does.
#   tests/upgrade_check.sh [REF]     (after ./build.sh here; REF's ./build.sh runs in its worktree)
set -e
cd "$(dirname "$0")/.."
REF=${1:-tessera-dev}
[ -f build/gen/melodee_tables.h ] || { echo "run ./build.sh first"; exit 1; }
TMP=$(mktemp -d)
trap 'git worktree remove --force "$TMP/tree" >/dev/null 2>&1; rm -rf "$TMP"' EXIT
git worktree add -q --detach "$TMP/tree" "$REF"
cp tests/upgrade_test.c "$TMP/tree/tests/"
( cd "$TMP/tree" && ./build.sh > "$TMP/build.log" 2>&1 ) || { tail -20 "$TMP/build.log"; exit 1; }
CC="${CC:-cc} -O1 -ffp-contract=off -w"
( cd "$TMP/tree" && mkdir -p build/host && $CC -Ibuild/gen -Ifirmware/src -o build/host/upgrade_test tests/upgrade_test.c -lm &&
  build/host/upgrade_test save "$TMP/old.nor" )
mkdir -p build/host
$CC -Ibuild/gen -Ifirmware/src -o build/host/upgrade_test tests/upgrade_test.c -lm
echo "== saved by $REF ($(git rev-parse --short "$REF")), loaded by $(git rev-parse --short HEAD)"
build/host/upgrade_test load "$TMP/old.nor"
