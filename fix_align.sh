#!/bin/bash
for i in {0..7}; do
    echo "Testing build with +$i padding variables..."
    PAD=""
    for ((j=0; j<i; j++)); do PAD="${PAD}P_TESS_R${j}, "; done
    sed -i "s/^[[:space:]]*P_TESS_PROB.*/    P_TESS_PROB, P_TESS_LEN, P_TESS_SPAN, P_TESS_DENS, P_TESS_MPH, ${PAD}/" firmware/src/core.h
    rm -rf build/
    if ./build.sh > build_log.txt 2>&1; then
        echo "Build SUCCESSFUL with +$i padding! Flashing firmware..."
        /mnt/c/Users/theo/AppData/Local/Programs/Python/Python312/python.exe tools/fm1_install.py build/melodee.fwsc
        exit 0
    fi
done
echo "All alignment attempts failed. Inspecting the exact assertion rule in the source:"
grep -A 2 "_Static_assert(sizeof(tmpl_t)" firmware/src/project.c
exit 1
