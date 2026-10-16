# Measuring Mutable Instruments' Plaits for the FM-1

Scripts used for `docs/design/phase4/PLAN.md` (2026-10-16). Plaits is MIT (Emilie Gillet); it is not in this
repo. Get it: `git clone https://github.com/pichenettes/eurorack E` and `git clone
https://github.com/pichenettes/stmlib E/stmlib` (the submodule), in a directory of its own.

- `compile.sh E OBJ shim`: every Plaits and stmlib source compiled for the AC79 (the JieLi clang 4, `-target
  pi32v2 -x c++`, the firmware's flags, no exceptions or RTTI). `shim/`: the few C++ headers Plaits uses (the
  toolchain has no C++ library). `-DTEST`: stmlib's portable paths in place of ARM assembly.
- `link.sh E B driver name`: a driver linked with the objects, unused sections dropped (`--gc-sections`);
  prints flash (text, rodata) and RAM (data, bss). `full.cc`: the whole voice. `mkdrv.py out models..`: a
  driver reaching only those models (va ws fm grain add wt chord speech swarm noise particle string modal bd
  sd hh vcf pd sixop terrain strmach chip).
- `hosttime.cc` (g++ on this computer) and `engtime.c` (gcc, with `tests/hostsim.c`): ns per sample of one
  voice, Plaits' models against the FM-1's engines on the same machine. Only the ratios mean anything; the
  device's own cost is measured on the device.
