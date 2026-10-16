#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (melodee-tess: tools/plaits_measure, see README.md)
# compile Plaits (and the stmlib it needs) for the FM-1's AC79 with the JieLi clang: objects in OBJ
E="$1"; OBJ="$2"; SHIM="$3"
TC=$HOME/.jieli/toolchain
mkdir -p "$OBJ"
FL="-mcpu=r3 -mfprev1 -ffp-contract=off -Os -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics -std=c++11 -w -I$SHIM -isystem $TC/pi32v2/include -I$E -include stdio.h -DNDEBUG -DTEST -DM_PI=3.14159265358979323846"
fail=0
for f in $(cd "$E" && ls plaits/dsp/*.cc plaits/dsp/*/*.cc plaits/resources.cc stmlib/dsp/*.cc stmlib/utils/*.cc 2>/dev/null); do
  o="$OBJ/$(echo $f | tr '/' '_' | sed 's/\.cc$/.o/')"
  if ! $TC/pi32v2/bin/clang -target pi32v2 -x c++ $FL -c "$E/$f" -o "$o" 2> "$o.err"; then echo "FAIL $f: $(head -3 $o.err)"; fail=1; fi
done
echo "compiled, fail=$fail"
