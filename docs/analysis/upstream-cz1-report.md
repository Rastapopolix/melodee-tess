# Draft: upstream issue for the CZ-1 knob test

Posted 2026-10-14 as https://github.com/keremimo/melodee/issues/47 (paragraphs unwrapped). Checked 2026-10-14 on upstream
`next` at `4fe8f0f` (1.4.0-rc.3), Ubuntu on WSL, x86-64. Text to paste below the line.

---

**Title:** `tests/cz1_knob_test.c`: does not build with GCC 14+, renders differ under GCC 15, dim knobs audible with clang

Hi, thanks for Melodee. While running the host tests on Linux (Ubuntu, x86-64, `next` at `4fe8f0f`, after
`./build.sh`) I hit three things in the CZ-1 knob test, the first a one-line fix.

**1. It does not compile with GCC 14 or newer.** `render()` takes `int16_t *dst`, but `ref` and `out` are
`int32_t[RENDER_N]`. GCC 14 made incompatible pointer types an error, so the test fails to build
(`tests/run_tests.sh` reports "compile tests/cz1_knob_test.c"). With older compilers it builds with a warning
(hidden by `-w`), and `render()` then writes 16-bit samples into the 32-bit buffers. The fix:

```diff
-static void render(track_t *t, int16_t *dst)
+static void render(track_t *t, int32_t *dst)
```

**2. With that fix and GCC 15.2 (`-O2 -ffp-contract=off`), two renders of the same tone differ:**
"renders are deterministic" fails, and so does every tone's "dim knobs leave the sound bit for bit" (8 FAIL). In
an earlier probe (on 1.1.0) two identical renders already differed from sample 11, even with the shared random
generator and the LFO phase reset between them, so some CZ-1 or voice state seems to survive `reset_all()` (or
something is undefined behaviour that GCC and clang treat differently).

**3. With clang (LLVM 24, same flags) the renders are deterministic**, but four factory tones still fail: knobs
drawn dim change the sound.

- MUSIC BOX: L5, L7 (73 dB)
- BELLS: R3 to R8, L7, L8 (up to 9 dB)
- JET ROAR: R3 to R7, L5, L7 (79 dB)
- LINE2 SHORT: RATE, DELAY, WAVE, WAVE2, W.KEY, A.KEY, L8, LINE1 (-0.0 dB: tiny but not bit for bit)

So either those knobs should not be dimmed for those tones, or the engine reads values it should ignore.

To reproduce:

```
./build.sh
cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/host/cz1 tests/cz1_knob_test.c -lm && build/host/cz1
clang -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/host/cz1c tests/cz1_knob_test.c -lm && build/host/cz1c
```

(I found this in my fork, melodee-tess, which carries the type fix; the results above are on unmodified `next` plus
that one line.)
