#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify closure, ABI and safe branches across XIP/SRAM for the IR pass."""
import sys
import unittest
import tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from audio_ram import relocate, FUNCTION, function_type, engine_roots

IR = '''define void @fm1_alnk0_irq() {
  call fastcc void @render(i32 32)
  store void (i8*)* @worker, void (i8*)** @callback
  ret void
}
define internal fastcc void @render(i32 %n) {
  call void @hot(i32 %n)
  ret void
}
define void @worker(i8* %context) {
  ret void
}
define void @hot(i32 %n) section ".dsp_text" {
  ret void
}
define void @boot() {
  call fastcc void @render(i32 32)
  ret void
}
define void @accept(void (i8*, i32)* %job, i8* %context) {
  ret void
}
@callback = global void (i8*)* null
'''

class AudioRamTest(unittest.TestCase):
    def test_closure_and_crossing(self):
        output, selected, far = relocate(IR, {'fm1_alnk0_irq'})
        self.assertEqual(set(selected), {'fm1_alnk0_irq','render','hot','worker'})
        self.assertEqual(far,['render'])
        self.assertIn('call fastcc void %audio_far_', output)
        self.assertIn('call void @hot(i32 %n)',output)
        self.assertIn('call fastcc void @render(i32 32)',output)
        self.assertIn('@__audio_far_render = internal global void (i32)* @render',output)
        functions={m[2]:m for m in FUNCTION.finditer(output)}
        self.assertIn('section ".audio_text"',functions['worker'][4])
        self.assertIn('section ".dsp_text"',functions['hot'][4])
        self.assertNotIn('section',functions['boot'][4])

    def test_callback_abi(self):
        fn=next(m for m in FUNCTION.finditer(IR) if m[2]=='accept')
        self.assertEqual(function_type(fn),'void (void (i8*, i32)*, i8*)')

    def test_missing_isr_rejected(self):
        with self.assertRaises(ValueError):relocate('', {'fm1_alnk0_irq'})

    def test_indirect_engine_callbacks(self):
        src=Path(__file__).resolve().parents[1]/'firmware/src'
        roots=engine_roots(src)
        self.assertIn('fm6_key',roots)
        self.assertIn('drum_keys',roots)
        self.assertNotIn('p5_preset_make',roots)

    def test_new_callback_requires_classification(self):
        with tempfile.TemporaryDirectory() as directory:
            src=Path(directory)
            (src/'core.h').write_text('typedef struct { void (*new_hook)(void); } engine_t;')
            with self.assertRaisesRegex(ValueError,'Unclassified engine callbacks'):
                engine_roots(src)

if __name__=='__main__':unittest.main()
