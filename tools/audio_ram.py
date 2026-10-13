#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Place the enabled audio call graph in SRAM after LLVM has finished inlining.

The resident profile places the complete dry audio graph in executable POOL SRAM.
Existing flash-safe and low-SRAM DSP sections keep their placement. Crossing
XIP/SRAM uses a volatile function pointer: pi32v2 direct branches cannot span
that distance. Calling conventions and function signatures stay unchanged.
"""
import json
import re
import sys
from pathlib import Path

FUNCTION = re.compile(r'^define ([^\n]+?)@([\w.]+)\(([^\n]*)\)([^\n]*)\{\n(.*?)^}', re.M | re.S)
REFERENCE = re.compile(r'@([\w.]+)')
CALL = re.compile(r'\bcall\b[^\n@]*@([\w.]+)\(')
ROOTS = {'fm1_alnk0_irq', 'audio_block', 'mix_block', 'ua_audio', 'click_render',
         'p5_worker_kernel', 'fm6_worker_kernel'}
CALLBACKS = {'note_on', 'render', 'amp', 'block', 'done', 'alloc', 'legato',
             'mono_key', 'post', 'units', 'cap', 'keys', 'desc'}
COLD_CALLBACKS = {'preset_make'}


def engine_roots(src):
    # Indirect engine calls are absent from LLVM's direct call graph. Check
    # the interface too, so a new callback cannot silently stay in flash.
    core = (src / 'core.h').read_text()
    engine = re.search(r'typedef struct\s*\{[^}]*\}\s*engine_t;', core, re.S)
    if not engine:
        raise ValueError('Engine callback interface is missing')
    fields = set(re.findall(r'\(\*(\w+)\)', engine[0]))
    unknown = fields - CALLBACKS - COLD_CALLBACKS
    if unknown:
        raise ValueError(f'Unclassified engine callbacks: {sorted(unknown)}')
    roots = set(ROOTS)
    for file in src.glob('eng_*.c'):
        for field, name in re.findall(r'\.(\w+)\s*=\s*(\w+)\s*[,}]', file.read_text()):
            if field in CALLBACKS:
                roots.add(name)
    return roots


def function_type(match):
    # Optimized definitions have scalar/pointer arguments; reject a new ABI
    # shape rather than silently constructing the wrong indirect call type.
    ret = match[1].strip().split()[-1]
    args = []
    depth = 0
    args_source = []
    begin = 0
    for pos, char in enumerate(match[3] + ','):
        depth += (char == '(') - (char == ')')
        if char == ',' and depth == 0:
            args_source.append(match[3][begin:pos].strip())
            begin = pos + 1
    for arg in filter(None, args_source):
        token = re.match(r'(?:void|i\d+|float|double|%[\w.]+)\**|\.\.\.', arg)
        if not token:
            raise ValueError(f'Unsupported audio argument type for {match[2]}: {arg}')
        end = token.end()
        rest = arg[end:].lstrip()
        if rest.startswith('('):
            start = len(arg) - len(rest)
            depth = 0
            for pos in range(start, len(arg)):
                depth += (arg[pos] == '(') - (arg[pos] == ')')
                if not depth:
                    end = pos + 1
                    while end < len(arg) and arg[end] == '*':
                        end += 1
                    break
        typ = arg[:end]
        args.append(typ)
    return f'{ret} ({", ".join(args)})'


def relocate(ir, roots):
    functions = {m[2]: m for m in FUNCTION.finditer(ir)}
    selected = set()
    pending = list(roots)
    while pending:
        name = pending.pop()
        if name not in functions or name in selected:
            continue
        selected.add(name)
        pending.extend(REFERENCE.findall(functions[name][5]))
    if 'fm1_alnk0_irq' not in selected:
        raise ValueError('Audio ISR is missing from the optimized module')

    sections = {name: (re.search(r'\bsection "([^"]+)"', m[4]) or [None, ''])[1]
                for name, m in functions.items()}
    for name in selected:
        if not sections[name]:
            sections[name] = '.audio_text'
        elif sections[name] not in ('.audio_text', '.dsp_text', '.ram_text'):
            raise ValueError(f'Unexpected audio section for {name}: {sections[name]}')
    far = {}

    def rewrite(match):
        name = match[2]
        header = match[0].split('\n', 1)[0]
        if name in selected and not re.search(r'\bsection "', match[4]):
            header = header[:-1].rstrip() + ' section ".audio_text" {'
        body = []
        for line in match[5].splitlines():
            call = CALL.search(line)
            if call and call[1] in functions and sections[name] != sections[call[1]]:
                # SRAM sections can call each other directly; all fit well
                # inside branch range. Only a flash/RAM crossing needs this.
                if bool(sections[name]) != bool(sections[call[1]]):
                    target = call[1]
                    typ = function_type(functions[target])
                    far[target] = typ
                    local = f'%audio_far_{len(body)}'
                    body.append(f'  {local} = load volatile {typ}*, {typ}** @__audio_far_{target}, align 4')
                    line = line[:call.start(1)-1] + local + line[call.end(1):]
            body.append(line)
        return header + '\n' + '\n'.join(body) + '\n}'

    result = FUNCTION.sub(rewrite, ir)
    globals_ir = '\n'.join(f'@__audio_far_{name} = internal global {typ}* @{name}, align 4'
                           for name, typ in sorted(far.items()))
    result += '\n' + globals_ir + '\n'
    return result, sorted(selected), sorted(far)


def main(source, destination):
    src = Path(__file__).resolve().parents[1] / 'firmware/src'
    roots = engine_roots(src)
    result, selected, far = relocate(Path(source).read_text(), roots)
    Path(destination).write_text(result)
    Path(destination).with_suffix('.json').write_text(json.dumps({'resident': selected, 'far_calls': far}, indent=2))
    print(f'audio RAM: {len(selected)} reachable functions; {len(far)} flash/RAM call boundaries')


if __name__ == '__main__':
    main(*sys.argv[1:3])
