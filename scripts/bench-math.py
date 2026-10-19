#!/usr/bin/env python3
"""Benchmark math APIs on one CPU; retain raw timings, codegen and metadata.

On Arm64, fixed and table math objects (and the timing driver) are compiled
with -mgeneral-regs-only. The tests/reference code may use FP; measured fixed
math cannot. Automatic bulk-loop vectorization is disabled; explicit NEON
kernels selected by the floating backend remain enabled.
"""
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import shlex
import tempfile

from mathcommon import (MODES, ROOT, build_includes, check_no_fp_simd,
                        compiler_type, run)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--cflags', default='')
    parser.add_argument('--cpu', type=int)
    parser.add_argument('--runs', type=int, default=1)
    parser.add_argument('--modes', nargs='+', choices=list(MODES),
                        default=list(MODES))
    parser.add_argument('--kernel', help='Optionally measure only this kernel')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error('--runs must be positive')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    cc = shlex.split(args.cc)
    version = run(cc + ['--version'], capture_output=True).stdout
    machine = run(cc + ['-dumpmachine'], capture_output=True).stdout.strip()
    arm64 = machine.startswith(('aarch64', 'arm64'))
    cpu = args.cpu
    if hasattr(os, 'sched_getaffinity'):
        allowed = os.sched_getaffinity(0)
        cpu = min(allowed) if cpu is None else cpu
        if cpu not in allowed:
            parser.error(f'CPU {cpu} not in allowed set {sorted(allowed)}')
    elif cpu is not None:
        parser.error('CPU pinning is unavailable on this platform')
    no_vector = (['-fno-vectorize', '-fno-slp-vectorize']
                 if compiler_type(version) == 'Clang'
                 else ['-fno-tree-vectorize', '-fno-tree-slp-vectorize'])
    common = ['-O2', *shlex.split(args.cflags), *no_vector, '-DCONFIG_WM_NONE=1']
    source_files = ['include/twin.h', 'include/twin_math.h', 'src/twin_private.h',
                    'src/fixed.c', 'src/trig.c', 'src/matrix.c', 'src/math-gen.h',
                    'src/math-trig.h', 'tests/math-bench.c', 'tests/math-bench-kernels.c',
                    'tests/math-codegen.c', 'scripts/gen-composite-decls.py']
    report = {'machine': platform.uname()._asdict(), 'compiler': version.strip(),
              'target': machine, 'cpu': cpu, 'common_flags': common,
              'source_sha256': {f: hashlib.sha256((ROOT / f).read_bytes()).hexdigest()
                                for f in source_files}, 'modes': {}}
    if Path('/proc/cpuinfo').exists():
        report['cpuinfo'] = Path('/proc/cpuinfo').read_text().split('\n\n')[0]
    if cpu is not None:
        for entry in ['scaling_governor', 'scaling_min_freq', 'scaling_max_freq']:
            path = Path(f'/sys/devices/system/cpu/cpu{cpu}/cpufreq/{entry}')
            if path.exists():
                report[entry] = path.read_text().strip()
    with tempfile.TemporaryDirectory(prefix='mado-bench-') as temp:
        build = Path(temp)
        includes = build_includes(build)
        integer_flags = ['-mgeneral-regs-only'] if arm64 else []
        for mode, policy in MODES.items():
            if mode not in args.modes:
                continue
            flags = common + policy + (integer_flags if mode != 'float' else [])
            objects = []
            for source in ['tests/math-bench.c', 'tests/math-bench-kernels.c',
                           'src/fixed.c', 'src/trig.c', 'src/matrix.c']:
                name = Path(source).stem
                obj = build / (mode + '-' + name + '.o')
                asm = output / (mode + '-' + name + '.s')
                # Assemble the inspected output so it is exactly what is timed.
                run(cc + flags + includes + ['-S', source, '-o', str(asm)])
                run(cc + shlex.split(args.cflags) + ['-c', str(asm), '-o', str(obj)])
                if arm64 and mode != 'float':
                    check_no_fp_simd(asm.read_text(), str(asm))
                objects.append(str(obj))
            probe = output / (mode + '-probes.s')
            run(cc + flags + includes + ['-S', 'tests/math-codegen.c', '-o', str(probe)])
            binary = build / mode
            run(cc + shlex.split(args.cflags) + [*objects, '-lm', '-o', str(binary)])
            measurements = []
            for repeat in range(args.runs):
                command = [str(binary)]
                if args.kernel:
                    command.append(args.kernel)
                if cpu is not None:
                    command = ['taskset', '-c', str(cpu), *command]
                raw = run(command, capture_output=True).stdout
                (output / f'{mode}-{repeat}.csv').write_text(raw)
                rows = []
                for row in csv.DictReader(io.StringIO(raw)):
                    row = {k: v if k == 'kernel' else int(v) for k, v in row.items()}
                    row['ns_per_op'] = row['median_ns'] / row['operations']
                    rows.append(row)
                measurements.append(rows)
                print(f'{mode}: run {repeat + 1}/{args.runs} complete', flush=True)
            report['modes'][mode] = {'flags': flags,
                                     'fp_simd_forbidden': arm64 and mode != 'float',
                                     'measurements': measurements}
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Results: {output / "results.json"}')


if __name__ == '__main__':
    main()
