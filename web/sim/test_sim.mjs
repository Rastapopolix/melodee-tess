// SPDX-License-Identifier: GPL-3.0-only
// The web simulator's firmware (build/sim/melodee.wasm, web/sim/build.sh) driven as worklet.js drives it (Felucca's
// web_* interface):
// it powers on and draws, a key sounds and stops, the demo plays on every track, the LEDs and the HOME scope
// follow, MASTER works, and it renders well ahead of real time. Run: node web/sim/test_sim.mjs [build/sim/melodee.wasm]
import fs from "fs";

const path = process.argv[2] || "build/sim/melodee.wasm";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(path), {});
const x = instance.exports;
x._initialize();
x.web_nor_erase();
x.web_master(700);
x.web_boot();
let peak = 0, fail = 0, draws = x.web_screen_draws();
const run = (blocks) => {
  for (let b = 0; b < blocks; b++) {
    x.web_render(128);
    for (const a of [x.web_out_l(), x.web_out_r()])
      for (const v of new Float32Array(x.memory.buffer, a, 128)) peak = Math.max(peak, Math.abs(v));
  }
};
const screen = () => new Uint16Array(x.memory.buffer, x.web_screen(), 240 * 240).slice();
const check = (what, ok) => { console.log(`sim: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); if (!ok) fail = 1; };
const S = 344;                                     // blocks of 128 per second
const PLAY = 1 << 10, C = 1 << 7;                  // panel.c B_PLAY (by label); key 7 = C4

run(S / 2);
check("powers on and draws the screen", x.web_screen_draws() !== draws && screen().some((v) => v));
check("silent at power-on (the sequencers start empty)", peak === 0);
peak = 0; x.web_keys(C); run(S / 2);
check("a key (C) plays the selected track", peak > 0.05);
x.web_keys(0); run(S * 2); peak = 0; run(S / 4);
check("the note ends after the key is let go", peak < 0.01);
const before = screen();
peak = 0; x.web_demo(); run(S * 4);
check("the demo plays (four Tessera generators, PLAY)", peak > 0.05 && x.web_playing() === 1);
check("the demo's EVOLVE on track 1: EVERY 2 bars, AMOUNT 30 %", x.web_test_evolve(0) === (2 << 8 | 30));
check("the PLAY button's LED is lit", (x.web_lit_buttons() & PLAY) !== 0);
check("the HOME screen changes while it plays (scope, playheads)", screen().some((v, i) => v !== before[i]));
check("the keys glow (the dim plane)", x.web_dim_keys() !== 0);
x.web_buttons(PLAY); run(4); x.web_buttons(0); run(S);
peak = 0; run(S / 2);
check("PLAY again stops it", peak < 0.02 && x.web_playing() === 0 && !(x.web_lit_buttons() & PLAY));
peak = 0; x.web_master(0); x.web_keys(C); run(S / 4); x.web_keys(0); x.web_master(700);
check("MASTER at 0: a key is silent (below -50 dB, the DC blocker settling)", peak < 0.003);
run(S); const home = screen(); x.web_enc(1, 1); run(S / 4);
check("ALGORITHM turns: another track selected (the screen changes)", screen().some((v, i) => v !== home[i]));
const t0 = performance.now(); x.web_demo(); run(S * 10); const dt = (performance.now() - t0) / 1000;
check(`10 s of audio and UI in ${dt.toFixed(2)} s (at least 4x real time)`, dt < 2.5);
process.exit(fail);
