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
windows running the demo apps.  The window manager is a build-time
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

Either build also runs with no window manager at all:

```shell
$ MADO_SCENE=none ./demo-sdl
```

`twin_screen_set_wm()` switches between the built policy and the 'none'
policy at run time.  Each demo app is registered in `apps/apps.c` with the
window managers it runs under: all three by default, the mobile one only
for the phone shell.  See [docs/phone.md](docs/phone.md) for the design of
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

## License

`Mado` is available under a MIT-style license, permitting liberal commercial use.
Use of this source code is governed by a MIT license that can be found in the [LICENSE](LICENSE) file.
