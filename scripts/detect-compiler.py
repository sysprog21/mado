#!/usr/bin/env python3
"""
Compiler Detection Script for Mado Build System

This script detects the compiler type (GCC, Clang, Emscripten) based on
CROSS_COMPILE and CC environment variables, following Linux kernel Kbuild
conventions.

Usage:
    python3 detect-compiler.py [--is COMPILER]

Default behavior (no arguments):
    Output compiler type (GCC/Clang/Emscripten/Unknown)

Options:
    --is COMPILER    Check if compiler matches specified type (GCC/Clang/Emscripten)
                     Returns exit code 0 if match, 1 otherwise
                     Example: --is Emscripten

Exit codes:
    0: Success (or match when using --is)
    1: Compiler not found, detection failed, or no match (when using --is)

Environment variables:
    CROSS_COMPILE: Toolchain prefix (e.g., "arm-none-eabi-")
    CC: C compiler command (e.g., "gcc", "clang", "emcc")
"""

import os
import shlex
import subprocess
import sys


def get_compiler_path():
    """
    Determine compiler path based on CROSS_COMPILE and CC variables.

    Priority:
    1. CC environment variable (explicit override)
    2. CROSS_COMPILE + gcc (when CC not set)
    3. Default: cc

    Returns:
        str or list: Compiler command to execute (may be list for wrapped compilers)
    """
    cross_compile = os.environ.get('CROSS_COMPILE', '')
    cc = os.environ.get('CC', '')

    # Priority: explicit CC overrides CROSS_COMPILE
    if cc:
        return cc
    elif cross_compile:
        return f"{cross_compile}gcc"
    else:
        return 'cc'


def get_compiler_version(compiler):
    """
    Get compiler version string.

    Args:
        compiler: Compiler command (string, may contain spaces for wrapped compilers)

    Returns:
        str: Full version output, or empty string if command fails
    """
    try:
        # Split command into argv tokens to support wrapped compilers like "ccache clang"
        cmd_argv = shlex.split(compiler) if isinstance(compiler, str) else [compiler]

        result = subprocess.run(
            cmd_argv + ['--version'],
            capture_output=True,
            text=True,
            timeout=5
        )
        return result.stdout if result.returncode == 0 else ''
    except (FileNotFoundError, subprocess.TimeoutExpired, ValueError):
        return ''


def detect_compiler_type(version_output):
    """
    Detect compiler type from version string.

    Priority order:
    1. Emscripten (check for "emcc")
    2. Clang (check for "clang" but not "emcc")
    3. GCC (check for "gcc" or "Free Software Foundation")
    4. Unknown

    Args:
        version_output: Compiler version string

    Returns:
        str: Compiler type (Emscripten/Clang/GCC/Unknown)
    """
    lower = version_output.lower()

    # Check Emscripten first (it may contain "clang" in output)
    if 'emcc' in lower:
        return 'Emscripten'

    # Check Clang (but not if it's Emscripten)
    if 'clang' in lower:
        return 'Clang'

    # Check GCC
    if 'gcc' in lower or 'free software foundation' in lower:
        return 'GCC'

    return 'Unknown'


def main():
    """Main entry point."""
    # Parse arguments
    check_is = '--is' in sys.argv

    # Get expected compiler type for --is check
    expected_type = None
    if check_is:
        try:
            is_index = sys.argv.index('--is')
            if is_index + 1 < len(sys.argv):
                expected_type = sys.argv[is_index + 1]
            else:
                print("Error: --is requires a compiler type argument", file=sys.stderr)
                sys.exit(1)
        except (ValueError, IndexError):
            print("Error: --is requires a compiler type argument", file=sys.stderr)
            sys.exit(1)

    # Get compiler path
    compiler = get_compiler_path()

    # Get version output
    version_output = get_compiler_version(compiler)

    if not version_output:
        print('Unknown')
        sys.exit(1)

    # Detect compiler type
    compiler_type = detect_compiler_type(version_output)

    # Handle --is option
    if check_is:
        # Case-insensitive comparison
        matches = compiler_type.lower() == expected_type.lower()
        sys.exit(0 if matches else 1)

    # Default: output compiler type
    print(compiler_type)
    sys.exit(0 if compiler_type != 'Unknown' else 1)


if __name__ == '__main__':
    main()
