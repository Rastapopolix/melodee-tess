# SPDX-License-Identifier: GPL-3.0-only
# a driver reaching only the named Plaits engines (their Init and Render): measure.py-style subset sizes
import sys
out, names = sys.argv[1], sys.argv[2:]
H = {'va':('virtual_analog_engine','VirtualAnalogEngine','engine'), 'ws':('waveshaping_engine','WaveshapingEngine','engine'),
     'fm':('fm_engine','FMEngine','engine'), 'grain':('grain_engine','GrainEngine','engine'),
     'add':('additive_engine','AdditiveEngine','engine'), 'wt':('wavetable_engine','WavetableEngine','engine'),
     'chord':('chord_engine','ChordEngine','engine'), 'speech':('speech_engine','SpeechEngine','engine'),
     'swarm':('swarm_engine','SwarmEngine','engine'), 'noise':('noise_engine','NoiseEngine','engine'),
     'particle':('particle_engine','ParticleEngine','engine'), 'string':('string_engine','StringEngine','engine'),
     'modal':('modal_engine','ModalEngine','engine'), 'bd':('bass_drum_engine','BassDrumEngine','engine'),
     'sd':('snare_drum_engine','SnareDrumEngine','engine'), 'hh':('hi_hat_engine','HiHatEngine','engine'),
     'vcf':('virtual_analog_vcf_engine','VirtualAnalogVCFEngine','engine2'), 'pd':('phase_distortion_engine','PhaseDistortionEngine','engine2'),
     'sixop':('six_op_engine','SixOpEngine','engine2'), 'terrain':('wave_terrain_engine','WaveTerrainEngine','engine2'),
     'strmach':('string_machine_engine','StringMachineEngine','engine2'), 'chip':('chiptune_engine','ChiptuneEngine','engine2')}
s = '#include "stmlib/utils/buffer_allocator.h"\n'
for n in names: s += '#include "plaits/dsp/%s/%s.h"\n' % (H[n][2], H[n][0])
s += 'char shared_buffer[16384];\nextern "C" void __cxa_pure_virtual() { for (;;) {} }\nextern "C" { void *__dso_handle; int __cxa_atexit(void (*)(void *), void *, void *) { return 0; } }\n'
for i, n in enumerate(names): s += 'static plaits::%s e%d;\n' % (H[n][1], i)
s += 'static float o[24], x[24];\nextern "C" int main() {\n  stmlib::BufferAllocator a(shared_buffer, sizeof shared_buffer);\n  plaits::EngineParameters p = {};\n  bool env;\n'
for i, n in enumerate(names): s += '  a.Free(); e%d.Init(&a); e%d.Render(p, o, x, 24, &env);\n' % (i, i)
s += '  return (int)o[0];\n}\n'
open(out, 'w').write(s)
