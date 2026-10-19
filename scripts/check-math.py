#!/usr/bin/env python3
"""Test every math backend; optionally inspect and execute RISC-V probes.

Examples:
  python3 scripts/check-math.py --sanitize
  python3 scripts/check-math.py --riscv-cc riscv64-linux-gnu-gcc --qemu qemu-riscv32
No RV32 libc/sysroot is needed for the freestanding RISC-V probes.
"""
import argparse
import math
import platform
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

from mathcommon import MODES, ROOT, build_includes, check_no_fp_simd, run


def function(assembly, name):
    match = re.search(r'^' + name + r':.*?(?=^\s*\.size\s+' + name + r'\b)',
                      assembly, re.MULTILINE | re.DOTALL)
    if not match:
        raise RuntimeError(f'Missing assembly function {name}')
    return match.group()


def instructions(body):
    return re.findall(r'^\s+([a-z][a-z0-9.]*)\b', body, re.MULTILINE)


def check_arm64_codegen(cc, flags, includes, directory, mode):
    base = shlex.split(cc) + ['-O2', '-DCONFIG_WM_NONE=1', *flags, *includes]
    if mode != 'float':
        base += ['-mgeneral-regs-only']
    output = directory / (mode + '-arm64.s')
    run(base + ['-S', 'tests/math-codegen.c', '-o', str(output)])
    code = output.read_text()
    product = function(code, 'math_product')
    quotient = function(code, 'math_quotient')
    if mode == 'float':
        if 'fcvtms' not in instructions(product):
            raise RuntimeError('Arm64 product: missing direct round-down conversion')
        if not re.search(r'fcvtzs[^\n]*#16', quotient):
            raise RuntimeError('Arm64 quotient: missing fixed-point conversion')
        for body in [product, quotient, function(code, 'math_compose')]:
            if any(op in ('bl', 'blr', 'b', 'cbz', 'cbnz', 'tbz', 'tbnz') or
                   op.startswith('b.') for op in instructions(body)):
                raise RuntimeError('Arm64 arithmetic: unexpected branch/helper call')
        if 'fsqrt' not in instructions(function(code, 'math_sqrt')):
            raise RuntimeError('Arm64 sqrt: missing hardware instruction')
    elif 'sdiv' not in instructions(quotient):
        raise RuntimeError('Arm64 fixed division: missing native SDIV')
    for source in ['src/fixed.c', 'src/trig.c', 'src/matrix.c']:
        run(base + ['-S', source, '-o', str(output)])
        code = output.read_text()
        if mode != 'float':
            check_no_fp_simd(code, source)
        elif source == 'src/fixed.c':
            for name in ['twin_fixed_sqrt', '_twin_xfixed_sqrt']:
                body = function(code, name)
                if 'fsqrt' not in instructions(body) or 'bl' in instructions(body):
                    raise RuntimeError(f'{name}: missing direct FSQRT or unexpected call')
    print(f'{mode}: Arm64 codegen and FP/SIMD policy passed', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc', help='Native compiler command')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--arm64', action='store_true',
                        default=platform.machine().lower() == 'aarch64',
                        help='Check A64 instructions and compile fixed cores without FP/SIMD')
    parser.add_argument('--riscv-cc', help='Optional RISC-V GCC or Clang command')
    parser.add_argument('--qemu', help='Optional qemu-riscv32 command')
    parser.add_argument('--kernels', action='store_true',
                        help='Also cross-compile trig/sqrt (needs target libc headers)')
    parser.add_argument('--kernel-cflags', default='',
                        help='Extra target header/sysroot flags for --kernels')
    args = parser.parse_args()
    if (args.qemu or args.kernels) and not args.riscv_cc:
        parser.error('--qemu/--kernels require --riscv-cc')
    if args.arm64:
        target = subprocess.check_output(shlex.split(args.cc) + ['-dumpmachine'], text=True)
        if not target.startswith('aarch64'):
            parser.error('--arm64 requires a native Arm64 compiler')
    with tempfile.TemporaryDirectory(prefix='mado-math-') as temp:
        directory = Path(temp)
        # Standalone checks must also work before the first Make invocation.
        includes = build_includes(directory)
        for output, flags in [('math-gen.h', []), ('math-trig.h', ['--trig-table'])]:
            generated = directory / output
            run(['python3', 'scripts/gen-math.py', *flags, '-o', str(generated)])
            if generated.read_bytes() != (ROOT / 'src' / output).read_bytes():
                raise RuntimeError(f'src/{output} is stale; regenerate it')
        sources = ['tests/test-math.c', 'src/fixed.c', 'src/trig.c', 'src/matrix.c']
        for name, flags in MODES.items():
            binary = directory / name
            command = shlex.split(args.cc) + ['-O2', '-Wall', '-Wextra', '-Werror',
                                              '-DCONFIG_WM_NONE=1']
            if args.sanitize:
                command += ['-fsanitize=undefined,float-cast-overflow',
                            '-fno-sanitize-recover=all']
            if args.arm64:
                objects = []
                for source in sources[1:]:
                    obj = directory / (name + '-' + Path(source).stem + '.o')
                    integer = ['-mgeneral-regs-only'] if name != 'float' else []
                    run(command + flags + integer + includes +
                        ['-c', source, '-o', str(obj)])
                    objects.append(str(obj))
                run(command + flags + includes + [sources[0], *objects, '-lm', '-o', str(binary)])
                check_arm64_codegen(args.cc, flags, includes, directory, name)
            else:
                run(command + flags + includes + sources + ['-lm', '-o', str(binary)])
            run([str(binary)])
            print(f'{name}: numeric, exhaustive trigonometry and matrix tests passed', flush=True)
        if not args.riscv_cc:
            return
        if args.kernels:
            # Independent double-precision reference for every trigonometry backend.
            values = [int(math.sin(i * math.tau / 4096) * 65536) for i in range(4096)]
            (directory / 'math-reference.h').write_text(
                'static const int reference_sine[4096] = {' +
                ','.join(map(str, values)) + '};\n')
        for isa, abi, flags in [('rv32i', 'ilp32', []), ('rv32im', 'ilp32', []),
                                ('rv32imf', 'ilp32f', ['-DCONFIG_MATH_FLOAT=1'])]:
            base = shlex.split(args.riscv_cc) + [
                '-O2', '-ffreestanding', '-fno-stack-protector', '-fno-pie',
                f'-march={isa}', f'-mabi={abi}', *flags, *includes]
            assembly = directory / (isa + '.s')
            run(base + ['-S', 'tests/math-codegen.c', '-o', str(assembly)])
            code = assembly.read_text()
            product = function(code, 'math_product')
            quotient = function(code, 'math_quotient')
            compose = function(code, 'math_compose')
            if isa == 'rv32im':
                if not {'mul', 'mulh'} <= set(instructions(product)):
                    raise RuntimeError('RV32IM product did not use M instructions')
            if isa == 'rv32imf':
                root = function(code, 'math_sqrt')
                if 'fsqrt.s' not in instructions(root):
                    raise RuntimeError('sqrt: missing hardware square root')
                if any(op in ('call', 'tail') for op in instructions(root)):
                    raise RuntimeError('sqrt: unexpected libm call')
                for name, body, rounding in [('product', product, 'rdn'),
                                              ('quotient', quotient, 'rtz')]:
                    if not re.search(r'fcvt\.w\.s[^\n]*\b' + rounding, body):
                        raise RuntimeError(f'{name}: missing direct {rounding} conversion')
                    if any(op.startswith('b') or op in ('call', 'tail')
                           for op in instructions(body)):
                        raise RuntimeError(f'{name}: unexpected branch or call')
                if any(op in ('call', 'tail') for op in instructions(compose)):
                    raise RuntimeError('Float matrix composition contains a call')
                for name, count in [('math_translate', 4), ('math_scale', 4),
                                    ('math_rotate', 8)]:
                    body = function(code, name)
                    if instructions(body).count('fcvt.w.s') != count:
                        raise RuntimeError(f'{name}: redundant product conversions')
                    if any(op in ('call', 'tail') for op in instructions(body)):
                        raise RuntimeError(f'{name}: unexpected call')
            counts = [len(instructions(body)) for body in (product, quotient, compose)]
            print(f'{isa}: codegen passed; product/quotient/compose assembly '
                  f'instructions = {counts}', flush=True)
            kernel_flags = []
            kernel_sources = []
            if args.kernels:
                kernel_flags = shlex.split(args.kernel_cflags) + ['-DCONFIG_WM_NONE=1']
                if isa == 'rv32i':
                    kernel_flags += ['-DCONFIG_MATH_TRIG_TABLE=1']
                for source, name in [('src/trig.c', 'twin_sincos'),
                                     ('src/fixed.c', 'twin_fixed_sqrt')]:
                    output = directory / (isa + '-' + name + '.s')
                    run(base + kernel_flags + ['-S', source, '-o', str(output)])
                    body = function(output.read_text(), name)
                    if any(op in ('call', 'tail') for op in instructions(body)):
                        raise RuntimeError(f'{isa} {name}: unexpected helper/libm call')
                kernel_sources = ['src/trig.c', 'src/fixed.c']
                kernel_flags += ['-DMADO_MATH_KERNEL_TEST=1', '-I' + temp]
                print(f'{isa}: sincos/sqrt kernels contain no helper/libm calls', flush=True)
            if args.qemu:
                binary = directory / isa
                # _start has no C runtime to initialize gp: disable gp relaxation.
                run(base + kernel_flags + ['-nostdlib', '-static', '-ffunction-sections',
                            '-Wl,--gc-sections,--no-relax',
                            '-DMADO_MATH_FREESTANDING_TEST=1', 'tests/math-codegen.c',
                            *kernel_sources, '-o', str(binary)])
                run(shlex.split(args.qemu) + [str(binary)])
                print(f'{isa}: QEMU arithmetic and aliasing tests passed', flush=True)


if __name__ == '__main__':
    main()
