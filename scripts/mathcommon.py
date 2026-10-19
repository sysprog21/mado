"""Helpers shared by check-math.py and bench-math.py."""
import importlib.util
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent

# Arithmetic backends and the defines that select them.
MODES = {
    'fixed': [],
    'table': ['-DCONFIG_MATH_TRIG_TABLE=1'],
    'float': ['-DCONFIG_MATH_FLOAT=1'],
}


def run(command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, text=True, **kwargs)


def build_includes(directory):
    """Generate composite-decls.h into directory; return the include flags."""
    with (Path(directory) / 'composite-decls.h').open('w') as generated:
        run(['python3', 'scripts/gen-composite-decls.py'], stdout=generated)
    return ['-Iinclude', '-Isrc', '-I' + str(directory)]


def check_no_fp_simd(code, what):
    """Reject Arm64 assembly that names an FP/SIMD register."""
    if re.search(r'\b[bhsdqv][0-9]+(?:\b|\.)', code):
        raise RuntimeError(f'{what}: unexpected FP/SIMD register')


def compiler_type(version_output):
    """Classify a --version banner with scripts/detect-compiler.py."""
    spec = importlib.util.spec_from_file_location(
        'detect_compiler', ROOT / 'scripts' / 'detect-compiler.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.detect_compiler_type(version_output)
