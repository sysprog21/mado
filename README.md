# Mado: A Window System for Resource-Constrained Devices

## Introduction

MADO (Minimalistic Application Display Orchestrator) is an open-source library
that brings advanced window system features to smaller devices. With embedded
systems now featuring high-resolution displays and powerful CPUs, sophisticated
graphical user interfaces can be realized even in the smallest devices. Although
CPU power has increased dramatically within a given power budget, these small
systems still face severe memory constraints. This unique environment presents
interesting challenges for graphical system design and implementation.

To address these challenges, `Mado` has been developed. Drawing from ideas used
in modern window systems for larger environments, `Mado` provides overlapping
translucent windows, anti-aliased graphics, drop shadow, animations, and scalable
fonts within a memory budget of several kilobytes. `Mado` also supports
[TinyVG](https://tinyvg.tech/), enabling efficient rendering of vector graphics
with a minimalistic set of drawing functions, without compromising visual quality.
`Mado` embeds window management directly into the toolkit, supporting resizing,
moving, and minimizing without external control. The toolkit automatically
constructs suitable decorations for each window as regular toolkit objects, and
the normal event dispatch mechanism directs window management activities.

`Mado` is a continuation of the work on [TWIN](https://keithp.com/~keithp/talks/twin-ols2005/),
originally developed by Keith Packard. 'Mado' means 'window' in the language of
the [Tao people](https://en.wikipedia.org/wiki/Tao_people) (also known as Yami),
an Austronesian ethnic group native to the tiny outlying Orchid Island of Taiwan.

## Principles of design

Focusing on specific hardware capabilities and limitations, `Mado` efficiently
utilizes limited resources. Over-constraining requirements can limit potential
target environments, but exploring reduced variation can be beneficial.

`Mado` assumes little-to-no graphics acceleration within the framebuffer, which
is connected to the CPU via a slow link. Therefore, most drawing is done in
local memory, not directly to the framebuffer, ensuring synchronized screen
updates and potentially reducing power consumption by minimizing off-chip data
references.

The system requires a color screen with fixed color mapping. This simplifies
software by allowing anti-aliased drawing to generate nearly invisible errors
instead of visibly incorrect results. Smooth color gradations eliminate the need
for dithering or other color-approximating schemes.

`Mado` is designed to ensure respectable CPU performance, which mitigates the
need for caching intermediate rendering results like glyph images for text. It
supports only a single, general performance class of drawing operations,
focusing exclusively on anti-aliased drawing. This focus is practical because
the CPUs that `Mado` supports are sufficiently fast, rendering non-anti-aliased
drawing obsolete. Moreover, `Mado` avoids reliance on a floating point unit (FPU)
or extensive FPU emulation by employing its own fixed-point arithmetic for all
necessary computations. This approach extends to nearly all of `Mado`'s optional
components as well. For specific tasks such as subpixel font layout and alpha
blending, `Mado` utilizes fixed-point math, ensuring that it remains efficient
even on CPUs without an FPU.

These environmental limitations enable `Mado` to provide significant
functionality with minimal wasted code. Unlike window systems designed for
varied platforms, `Mado` avoids unnecessary generalization and variability,
benefiting the entire application stack.

## Build and Verify

### Prerequisites

`Mado` is built with a minimalist design in mind. However, its verification
relies on certain third-party packages for full functionality and access to all
its features. We encourage the development environment to be installed with all optional
packages, including [libjpeg](https://www.ijg.org/) and [libpng](https://github.com/pnggroup/libpng).
* macOS: `brew install jpeg libpng`
* Ubuntu Linux / Debian: `sudo apt install libjpeg-dev libpng-dev`

The renderer implementation can either use the built-in pixel manipulation or be based on [Pixman](https://pixman.org/).
The built-in renderer is simple and performs adequately on platforms without SIMD instructions,
such as RISC-V processors without vector extensions.
However, for modern CPU architectures with extensive ISA extensions, Pixman offers faster pixel manipulation.
Install [Pixman](https://pixman.org/) before selecting the corresponding renderer implementation.
* macOS: `brew install pixman`
* Ubuntu Linux / Debian: `sudo apt install libpixman-1-dev`

### Graphics Backend

`Mado` supports multiple graphics backends. Choose one based on your deployment scenario:
- SDL: Cross-platform desktop development with hardware acceleration
- WebAssembly: Lightweight browser deployment using native Canvas API
- Linux DRM (drm): Modern Linux display management for embedded systems
- Linux framebuffer (fbdev): Direct hardware access for embedded Linux
- VNC: Remote display via Virtual Network Computing protocol
- Headless: Testing and automation without display output

`Mado` uses SDL2 as the primary development and demonstration backend, providing cross-platform graphics with hardware acceleration.
For web deployment, a dedicated WebAssembly backend directly uses the browser's Canvas API without SDL dependency, significantly reducing binary size. For all backend options and detailed build instructions, see [docs/backends.md](docs/backends.md).

Install the [SDL2 library](https://www.libsdl.org/):
* macOS: `brew install sdl2`
* Ubuntu Linux / Debian: `sudo apt install libsdl2-dev`

### Configuration

`Mado` uses [Kconfiglib](https://github.com/sysprog21/Kconfiglib) for flexible build configuration.

For quick setup with defaults (SDL backend enabled):
```shell
$ make defconfig
```

For interactive configuration (to explore all options):
```shell
$ make config
```

### Build and Execution

Build the library and demo program:

```shell
$ make
```

Run the demo program:

```shell
$ ./demo-sdl
```

The default build uses the desktop window manager: overlapping, decorated
windows running the applications.  The window manager is a build-time
choice, and the desktop and mobile policies are mutually exclusive.  Build
the phone instead, a lock screen (drag the knob to unlock), a home screen
and a dozen apps with vector-drawn interfaces (Photos shows JPEG
fixtures) on the mobile window manager; the launcher also lists every demo
app that is built:

```shell
$ make defconfig
$ python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig WM_MOBILE=y
$ python3 tools/kconfig/genconfig.py configs/Kconfig
$ make
$ ./demo-sdl                      # phone; drag from the left edge to go back
```

`WM_NONE=y` builds with no window manager at all, showing a single app.
The window manager is fixed for the life of the build: nothing switches
or checks it at run time.  The applications in `apps/apps.c` run under all
three; the phone shell depends on `WM_MOBILE` and is built only there.  See [docs/phone.md](docs/phone.md) for the design of
the window manager, compositor and caches.

### Verification

Headless test suites run in-process with a virtual clock.  At checkpoints
they check the incrementally updated frame against a full recomposite, and
the core and phone suites also compare golden frame checksums:

```shell
$ make check          # core, window manager, lifetime (and phone) suites
$ make check-update   # re-record golden frame checksums
```

For information about other backends (Linux framebuffer, VNC, headless), see [docs/backends.md](docs/backends.md).

## Arithmetic and RISC-V targets

`make config` offers an arithmetic backend and an independent ISA profile:

| ISA profile | Compiler flags | Suggested arithmetic |
| --- | --- | --- |
| Compiler default | Toolchain defaults | Fixed, or float with an FPU |
| RV32I | `-march=rv32i -mabi=ilp32` | Fixed |
| RV32IM | `-march=rv32im -mabi=ilp32` | Fixed |
| RV32IMF | `-march=rv32imf -mabi=ilp32f` | Float |

The RISC-V profiles appear only when the configured compiler targets RISC-V,
so pass the cross toolchain to both steps: `make CROSS_COMPILE=riscv32-unknown-linux-gnu- config`,
then `make CROSS_COMPILE=riscv32-unknown-linux-gnu- libtwin.a`. Use a matching
C library.
The ISA flags apply to both compilation and linking. RV32I uses compiler
runtime helpers for multiplication/division; RV32IM can use M instructions.
RV32IMF float arithmetic uses single precision, without requiring the D extension.
The selected ABI must also match any external libraries.

The default `CONFIG_MATH_FIXED` preserves integer Q16.16 arithmetic.
`CONFIG_MATH_FLOAT` selects binary32 multiplication, division, square root and
trigonometry and links libm for square root/inverse cosine. Sine/cosine share
integer quadrant reduction and an inline degree-7 minimax polynomial; their exhaustive
angle test allows at most two Q16.16 LSBs of absolute error. RV32IMF uses FMA and
direct `fcvt.w.s` rounding/saturation, without conversion fixup branches.
Its guarded positive square root emits `fsqrt.s` directly, with no libm errno
fallback. These RV32IMF operations require only the F extension. Arm64 has
separate native conversion, FSQRT and NEON kernels; other architectures retain
portable C implementations. The rounding and saturation behavior follows
the [RISC-V F specification](https://docs.riscv.org/reference/isa/unpriv/f-st-ext.html). Public coordinates remain Q16.16 integers; raster
coverage and Q31.32 storage retain integer semantics. Arm64 float builds use a
binary64 square-root seed followed by exact integer correction, preserving
legacy Q31.32 square-root results; wide multiplication/division remain integer. This avoids
changing font formats or the coordinate ABI. Float arithmetic is approximate:
its 24 significant bits cannot preserve every Q16.16 value at large magnitudes,
and rendered output can differ. Finite, representable inputs/results and
nonzero divisors are required; float conversion saturates at the storage limits.
Run `make clean` when switching arithmetic or ISA settings, then rebuild all
application code with the same generated `config.h` (or force-include
it as the Makefile does), because arithmetic helpers are inline in the public API.

Following b3d's generated-math approach, `scripts/gen-math.py` emits
`src/math-gen.h` with unrolled affine matrix composition and specialized
translation, scaling and rotation. The specialized kernels remove structural
zero/one products and preserve unaffected coefficients exactly. Both backends
share the formulas, preserve per-product quantization, and support aliased
matrix outputs. Make regenerates it when the generator changes. Generated code uses
ordinary inline C so the compiler can fold constants and choose target
instructions, with no runtime dispatch or initialization.

`make check` includes arithmetic, trigonometry and affine composition regression
tests in addition to the rendering suite. Run it once per configured backend.
Golden frame checksums record the arithmetic backend and sine implementation,
because both change pixels; they are kept for the default fixed-point table
build, and other math builds skip those comparisons.

Fixed sine/cosine read a generated 4100-byte quarter-wave table by default
(`CONFIG_MATH_TRIG_TABLE`), rounded to the nearest Q16 value (at most half an
LSB error), with no multiplies or initialization. The generator uses 80-digit
decimal arithmetic, independent of host libm. Disabling the table, which
`CONFIG_OPTIMIZE_SIZE` does by default, selects a bounded 32-bit polynomial
instead; its endpoint-constrained degree-5 coefficients keep error below six
Q16 LSBs and avoid 64-bit multiply helpers.
Table timings below assume cached reads; on cores without a data cache,
each lookup pays flash wait states, so measure there if speed matters.
Both fixed implementations preserve exact cardinal values; their outputs differ
from the previous polynomial, which was the only fixed implementation before.
Exhaustive tests check every angle, including negative angles, single-output
calls and first-quadrant monotonicity.

On eMag (GCC 13.3, `-O2 -mcpu=emag`, CPU 2, seven runs), both fixed
implementations compare against the previous polynomial as follows:

| Fixed implementation | Maximum error (Q16 LSB) | Time (ns/call) |
| --- | ---: | ---: |
| Previous polynomial (baseline) | 13.0214 | 22.289 |
| Quarter-wave table (default) | 0.49945 | 6.120 |
| Polynomial (table disabled) | 5.6437 | 22.275 |

Times are medians of seven per-run medians for calls computing both outputs.
The new coefficients reduce polynomial error without measurable cost; Arm64 and
RV32IM polynomial instruction streams differ from the baseline only in
coefficient immediates. The 4100-byte table is new, so it has no earlier
instruction stream to compare. Neither fixed core uses FP instructions. Raw
measurements, source hashes, accuracy bounds and validation evidence are
retained in
[`tests/results/emag-sincos-accuracy.json`](tests/results/emag-sincos-accuracy.json);
its `before` table entry is an unreleased table that reproduced the previous
polynomial's samples.
The 4096-step angular resolution is unchanged; these error bounds compare outputs
at representable angles, excluding quantization of an arbitrary input angle.

Square-root normalization
uses leading-zero counting once, without a bit-at-a-time normalization loop.

Run `make check-math` for all three arithmetic implementations with sanitizers.
The standalone checker also validates generated headers and offers repeatable
RISC-V assembly checks and libc-free QEMU execution:

```sh
python3 scripts/check-math.py --sanitize \
    --riscv-cc riscv64-linux-gnu-gcc --qemu qemu-riscv32
```

The RISC-V probes check scalar products/division and generated matrix composition,
including saturated float conversion, negative rounding and aliased output.
They require no RV32 libc; the full renderer still requires a matching sysroot.
Assembly instruction counts describe compiler output, not hardware cycle counts.

With target libc headers installed, add `--kernels` to inspect and execute the
actual sine/cosine and square-root implementations too. Optional
`--kernel-cflags` supplies target header or sysroot flags. This mode checks for
unexpected helper/libm calls and compares the FMA sine/cosine output across the
full circle against an independent double-precision reference. The default
probes and all QEMU executables link without libc.

### Arm64 verification on eMag

The eMag measurements below use GCC 13.3, `-O2 -mcpu=emag`, CPU 2 affinity,
three process runs and seven timed batches per kernel/run. Each entry is the
median of the three per-run medians, in ns per operation. Calibration warms the
CPU and caches; the governor remains `ondemand`. Automatic bulk-loop
vectorization is disabled in all modes; the float backend's explicit NEON
matrix and paired sine/cosine kernels remain enabled. Fixed/table math objects
and the timing driver use `-mgeneral-regs-only`, and their assembly is checked
for FP/SIMD registers. The integer libgcc division helper was also audited.
The fixed column uses the default sine table; disabling it changes only the
sine/cosine and rotation rows (other kernels differ by under 0.1%).

| Operation | Fixed, no FP/SIMD | Float + NEON |
| --- | ---: | ---: |
| Q16.16 multiply | 1.16 | 2.91 |
| Q16.16 general divide | 12.73 | 6.73 |
| Q16.16 square root | 77.18 | 17.69 |
| Sine + cosine, table (default) | 6.12 | 23.11 |
| Sine + cosine, polynomial | 22.28 | — |
| Matrix rotation, table (default) | 15.99 | 35.51 |
| Matrix rotation, polynomial | 31.46 | — |
| Inverse cosine | 146.45 | 50.07 |
| Q31.32 general divide | 40.17 | 40.08 |
| Q31.32 whole-divisor divide | 12.71 | 12.70 |
| Q31.32 square root | 143.93 | 26.85 |
| Matrix composition | 11.63 | 10.38 |
| Point transform | 5.21 | 13.78 |

The floating matrix composition improved from 189.88 to 10.38 ns/op in this
optimization pass. Native FP-register conversion avoids unnecessary GP/FP
transfers; two-column NEON kernels round each product before integer addition.
The whole-divisor Q31.32 fast path improved from 34.25 to 12.70 ns/op. General
Q31.32 division retains the compiler's exact 128-bit division helper when a
64-bit shortcut is unavailable. Fixed arithmetic/table lookup remains faster
for several operations, so select the backend for the actual workload.

Reproduce on the Arm64 host:

```sh
python3 scripts/check-math.py --cc gcc --sanitize --arm64
python3 scripts/check-math.py --cc clang --sanitize --arm64
python3 scripts/bench-math.py --cc gcc --cflags=-mcpu=emag --cpu 2 --runs 3 \
    --output /tmp/mado-emag-results
```

`make check-math` automatically enables Arm64 code-generation and integer-only
core checks on Linux AArch64. The numerical tests cover signed range boundaries,
saturation, exhaustive sine/cosine and inverse cosine, independent 128-bit
arithmetic/96-bit square-root oracles, and matrix aliasing. The eMag runs passed
with GCC, Clang and FMA contraction disabled, plus all 516 existing renderer
checks in the default table, polynomial and float builds. RISC-V
code-generation/QEMU regressions also pass. Full measurements, build flags, source hashes and validation records
are retained in [the eMag results](tests/results/emag-math.json).

## License

`Mado` is available under a MIT-style license, permitting liberal commercial use.
Use of this source code is governed by a MIT license that can be found in the [LICENSE](LICENSE) file.
