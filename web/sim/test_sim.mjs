// SPDX-License-Identifier: GPL-3.0-only
// The web simulator's firmware (build/sim/melodee.wasm, web/sim/build.sh) driven as sim-worklet.js drives it:
// it powers on and draws, a key sounds and stops, the demo plays on every track, the HOME scope moves, and
// it renders well ahead of real time. Run: node web/sim/test_sim.mjs [build/sim/melodee.wasm]
import fs from "fs";

const path = process.argv[2] || "build/sim/melodee.wasm";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(path), {});
const x = instance.exports;
x._initialize();
x.sim_init();
const FRAME = 44100 * 0.016;
let acc = 0, peak = 0, changed = 0, fail = 0;
const run = (blocks) => {
  for (let b = 0; b < blocks; b++) {
    const f = new Float32Array(x.memory.buffer, x.sim_render(128), 256);
    for (const v of f) peak = Math.max(peak, Math.abs(v));
    acc += 128;
    while (acc >= FRAME) { acc -= FRAME; changed += x.sim_frame(); }
  }
};
const screen = () => new Uint8Array(x.memory.buffer, x.sim_screen(), 240 * 240 * 2).slice();
const check = (what, ok) => { console.log(`sim: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); if (!ok) fail = 1; };
const S = 344;                                     // blocks of 128 per second

run(S / 2);
check("powers on and draws the screen", changed > 0 && screen().some((v) => v));
check("silent at power-on (the sequencers start empty)", peak === 0);
peak = 0; x.sim_key(7, 1); run(S / 2);
check("a key (C) plays the selected track", peak > 0.05);
x.sim_key(7, 0); run(S * 2); peak = 0; run(S / 4);
check("the note ends after the key is let go", peak < 0.01);
const before = screen();
peak = 0; x.sim_demo(); run(S * 4);
check("the demo plays (four Tessera generators, PLAY)", peak > 0.05);
const leds = new Uint8Array(x.memory.buffer, x.sim_leds(), 41);
check("the PLAY button's LED is lit", leds[x.sim_button_id(10)] === 2);
check("the HOME screen changes while it plays (scope, playheads)", screen().some((v, i) => v !== before[i]));
x.sim_button(10, 1); run(4); x.sim_button(10, 0); run(S);
peak = 0; run(S / 2);
check("PLAY again stops it", peak < 0.02 && new Uint8Array(x.memory.buffer, x.sim_leds(), 41)[x.sim_button_id(10)] !== 2);
const t0 = performance.now(); x.sim_demo(); run(S * 10); const dt = (performance.now() - t0) / 1000;
check(`10 s of audio and UI in ${dt.toFixed(2)} s (at least 4x real time)`, dt < 2.5);
process.exit(fail);
