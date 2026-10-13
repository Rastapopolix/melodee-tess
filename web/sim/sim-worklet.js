// SPDX-License-Identifier: GPL-3.0-only
// Web simulator: the firmware (melodee.wasm, web/sim/sim.c) runs here, on the audio thread, as on the device:
// audio blocks drive the clock and a UI frame runs every 16 ms of audio. Messages in: buttons, keys, knobs,
// MIDI, demo. Out: the screen (240 x 240 RGB565, high byte first) when it changed, the LEDs, the load.
const FRAME = sampleRate * 0.016;            // a UI frame (main.c: ~60 frames/s at most)

class MelodeeSim extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.x = null;
    this.acc = 0;
    this.queue = [];
    this.leds = new Uint8Array(41);
    this.busy = 0;
    this.blocks = 0;
    WebAssembly.instantiate(options.processorOptions.wasm, {}).then(({ instance }) => {
      const x = instance.exports;
      x._initialize();
      x.sim_init();
      this.x = x;
      this.port.postMessage({ t: "ready" });
    }, (e) => this.port.postMessage({ t: "error", msg: String(e) }));
    this.port.onmessage = (e) => this.queue.push(e.data);
  }

  input(m) {
    const x = this.x;
    switch (m.t) {
      case "btn": x.sim_button(m.label, m.down ? 1 : 0); break;
      case "key": x.sim_key(m.k, m.down ? 1 : 0); break;
      case "turn": x.sim_turn(m.role, m.steps); break;
      case "midi": x.sim_midi(m.d[0], m.d[1] || 0, m.d[2] || 0); break;
      case "demo": x.sim_demo(); break;
    }
  }

  frame() {
    const x = this.x, mem = x.memory.buffer;
    const changed = x.sim_frame();
    if (changed) {
      const px = new Uint8Array(mem, x.sim_screen(), 240 * 240 * 2).slice();
      this.port.postMessage({ t: "screen", px }, [px.buffer]);
    }
    const leds = new Uint8Array(mem, x.sim_leds(), 41);
    let diff = false;
    for (let i = 0; i < 41; i++) if (leds[i] !== this.leds[i]) { diff = true; break; }
    if (diff) {
      this.leds.set(leds);
      this.port.postMessage({ t: "leds", leds: this.leds.slice() });
    }
  }

  process(inputs, outputs) {
    const out = outputs[0], n = out[0].length;
    if (!this.x) return true;
    const t0 = Date.now();
    while (this.queue.length) this.input(this.queue.shift());   // between blocks, like the scan between renders
    const p = this.x.sim_render(n);
    const s = new Float32Array(this.x.memory.buffer, p, 2 * n);
    const l = out[0], r = out[1] || out[0];
    for (let i = 0; i < n; i++) {
      l[i] = s[2 * i];
      r[i] = s[2 * i + 1];
    }
    this.acc += n;
    if (this.acc >= FRAME) {
      this.acc -= FRAME;
      this.frame();
    }
    this.busy += Date.now() - t0;
    if (++this.blocks === 344) {                 // about once a second: the share of real time spent here
      this.port.postMessage({ t: "load", pct: Math.round(this.busy / (344 * n / sampleRate * 1000) * 100) });
      this.busy = this.blocks = 0;
    }
    return true;
  }
}
registerProcessor("melodee-sim", MelodeeSim);
