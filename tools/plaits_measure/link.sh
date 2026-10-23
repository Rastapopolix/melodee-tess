#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (melodee-tess: tools/plaits_measure, see README.md)
# compile a driver, link it with the Plaits objects (unused sections dropped), print text/rodata/data/bss
E="$1"; B="$2"; DRV="$3"; NAME="$4"
TC=$HOME/.jieli/toolchain
FL="-mcpu=r3 -mfprev1 -ffp-contract=off -Os -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics -std=c++11 -w -I$B/shim -isystem $TC/pi32v2/include -I$E -include stdio.h -DNDEBUG -DTEST -DM_PI=3.14159265358979323846"
$TC/pi32v2/bin/clang -target pi32v2 -x c++ $FL -c "$DRV" -o "$B/$NAME.o" || exit 1
$TC/pi32v2/bin/ld --gc-sections -e main "$B/$NAME.o" $(if [ "$NAME" = full ]; then ls $B/obj/*.o; else ls $B/obj/*.o | grep -v plaits_dsp_voice.o; fi) \
   $TC/pi32v2/lib/r3/libm.a $TC/pi32v2/lib/r3/libc.a $TC/pi32v2/lib/libcompiler-rt.a -o "$B/$NAME.elf" 2> "$B/$NAME.lderr" || { head -5 "$B/$NAME.lderr"; exit 1; }
$TC/pi32/bin/objdump -h "$B/$NAME.elf" | awk -v n="$NAME" '$2 ~ /^\./ {s=strtonum("0x"$3); if ($2 ~ /bss/) b+=s; else if ($2 ~ /data/ && $2 !~ /rodata/) d+=s; else if ($2 ~ /text|rodata|init_array|ctors/) t+=s}
  END {printf "%-28s flash %7d  RAM %7d (data %d, bss %d)\n", n, t+d, d+b, d, b}'
