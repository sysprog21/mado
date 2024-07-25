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
translucent windows, anti-aliased graphics, and scalable fonts within a memory
budget of several kilobytes. `Mado` embeds window management directly into the
toolkit, supporting resizing, moving, and minimizing without external control.
The toolkit automatically constructs suitable decorations for each window as
regular toolkit objects, and the normal event dispatch mechanism directs window
management activities.

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

Respectable CPU performance is expected, reducing the need to cache intermediate
rendering results, such as glyph images for text. Supporting only one general
performance class of drawing operations, `Mado` focuses on anti-aliased drawing,
as supported CPUs are fast enough to make non-anti-aliased drawing irrelevant.
Additionally, `Mado` does not rely on an FPU or heavy FPU emulations, using its
own fixed-point arithmetic instead.

These environmental limitations enable `Mado` to provide significant
functionality with minimal wasted code. Unlike window systems designed for
varied platforms, `Mado` avoids unnecessary generalization and variability,
benefiting the entire application stack.

## Build and Verify

`Mado` is built with a minimalist design in mind. However, its verification
relies on certain third-party packages for full functionality and access to all
its features. To ensure proper operation, the development environment should
have the [SDL2 library](https://www.libsdl.org/), [libjpeg](https://www.ijg.org/), and [libpng](https://github.com/pnggroup/libpng) installed.
* macOS: `brew install sdl2 jpeg libpng`
* Ubuntu Linux / Debian: `sudo apt install libsdl2-dev libjpeg-dev libpng-dev`

Configure via [Kconfiglib](https://pypi.org/project/kconfiglib/)
```shell
$ make config
```

Build the library and demo program.
```shell
$ make
```

Run sample `Mado` program:
```shell
$ ./demo-sdl
```

Once the window appears, you should be able to move the windows and interact with the widgets.

## License

`Mado` is available under a MIT-style license, permitting liberal commercial use.
Use of this source code is governed by a MIT license that can be found in the [LICENSE](LICENSE) file.
