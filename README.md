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

`Mado` supports multiple graphics backends. Choose one based on your deployment scenario:
- SDL: Cross-platform desktop development with hardware acceleration
- Linux framebuffer (fbdev): Direct hardware access for embedded Linux
- VNC: Remote display via Virtual Network Computing protocol
- Headless: Testing and automation without display output

### Backend-Specific Dependencies

For SDL backend, install the [SDL2 library](https://www.libsdl.org/).
* macOS: `brew install sdl2`
* Ubuntu Linux / Debian: `sudo apt install libsdl2-dev`

For DRM backend, install the [libdrm](https://gitlab.freedesktop.org/mesa/drm).
* Ubuntu Linux / Debian: `sudo apt install libdrm-dev`

For the VNC backend, please note that it has only been tested on GNU/Linux, and the prebuilt [neatvnc](https://github.com/any1/neatvnc) package might be outdated. To ensure you have the latest version, you can build the required packages from source by running the script:
```shell
$ tools/build-neatvnc.sh
```

For Linux framebuffer backend, install `libudev` and `libuuid`:
* Ubuntu Linux / Debian: `sudo apt install libudev-dev uuid-dev`

For the headless backend, no additional dependencies are required. This backend uses shared memory for rendering and can be controlled programmatically via `headless-ctl`.

For detailed information about each backend's features, use cases, and configuration options, see [docs/backends.md](docs/backends.md).

### Configuration

`Mado` uses [Kconfiglib](https://github.com/sysprog21/Kconfiglib) for flexible build configuration. Choose from multiple graphics backends: SDL, Linux framebuffer, VNC, or headless (for testing/automation).

For interactive configuration with a terminal menu:
```shell
$ make config
```

For quick scripted configuration (recommended for most users):
```shell
$ make defconfig
$ python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig \
    BACKEND_SDL=y \
    DEMO_MULTI=y
```

For detailed configuration options and advanced usage, see [docs/kconfig-usage.md](docs/kconfig-usage.md).

### Build and execution

Build the library and demo program:

```shell
$ make
```

To run demo program with SDL backend:

```shell
$ ./demo-sdl
```

Once the window appears, you should be able to move the windows and interact with the widgets.

To run demo program with the Linux framebuffer backend:

```shell
$ sudo ./demo-fbdev
```

Normal users don't have access to `/dev/fb0` so require `sudo`. Alternatively, you can add the user to the video group to avoid typing `sudo` every time:

```shell
$ sudo usermod -a -G video $USERNAME
```

In addition, the framebuffer device can be assigned via the environment variable `FRAMEBUFFER`.

To run demo program with the DRM backend:

```shell
$ sudo ./demo-drm
```

The DRM device can be assigened via the environment variable `DRI_CARD`.

To run demo program with the VNC backend:

```shell
$ ./demo-vnc
```

This will start the VNC server. You can use any VNC client to connect using the specified IP address (default is `127.0.0.1`) and port (default is `5900`).
The IP address can be set using the `MADO_VNC_HOST` environment variable, and the port can be configured using `MADO_VNC_PORT`.

To run demo program with the headless backend (for testing/automation):

```shell
$ ./demo-headless &
$ ./headless-ctl status           # Check backend status
$ ./headless-ctl shot output.png  # Capture screenshot
$ ./headless-ctl shutdown         # Graceful shutdown
```

The headless backend uses shared memory for rendering without display output. Use `headless-ctl` to monitor, control, and capture screenshots.
Ideal for CI/CD pipelines, automated testing, and memory debugging.
See [docs/backends.md](docs/backends.md) for advanced usage including event injection and live monitoring.

## License

`Mado` is available under a MIT-style license, permitting liberal commercial use.
Use of this source code is governed by a MIT license that can be found in the [LICENSE](LICENSE) file.
