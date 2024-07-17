# Mado: A Window System for Resource-Constrained Devices

With embedded systems now featuring high-resolution displays and powerful CPUs,
the desire for sophisticated graphical user interfaces can be realized even in
the smallest devices. Although CPU power within a given power budget has
increased dramatically, these small systems still face severe memory
constraints. This unique environment presents interesting challenges for
graphical system design and implementation. To address these challenges,
a new window system called `Mado` has been developed. Drawing from ideas used in
modern window systems for larger environments, `Mado` provides overlapping
translucent windows, anti-aliased graphics, and scalable fonts within a total
memory budget of several kilobytes.

`Mado` embeds window management directly into the toolkit. Support for resizing,
moving, and minimizing is not under the control of an external application.
Instead, the toolkit automatically constructs suitable decorations for each
window as regular toolkit objects, and the normal event dispatch mechanism
directs window management activities.

While external management is a valuable architectural feature in a
heterogeneous desktop environment, the additional space, time, and complexity
rule this out in diverse embedded systems.

`Mado` is a continuation of the work on [TWIN](https://keithp.com/~keithp/talks/twin-ols2005/),
originally developed by Keith Packard. 'Mado' means 'window' in the language of
the [Tao people](https://en.wikipedia.org/wiki/Tao_people) (also known as Yami),
an Austronesian ethnic group native to the tiny outlying Orchid Island of Taiwan.

## Assumptions

By focusing on specific hardware capabilities and limitations, the window
system will more effectively utilize those limited resources. Of course,
over-constraining the requirements can limit the potential target
environments. Given the very general nature of existing window systems, it
seems interesting to explore what happens when less variation is permitted.

The first assumption is that little-to-no graphics acceleration is available
within the framebuffer and that the framebuffer is attached to the CPU through
a relatively slow link. This combination means that most drawing should be done
with the CPU in local memory, not directly to the framebuffer. This has the
additional benefit of encouraging synchronized screen updates where intermediate
rendering results are never made visible to the user. If the CPU has sufficient
on-chip storage, this design can also reduce power consumption by minimizing
off-chip data references.

The second limitation imposed is the requirement of a color screen with fixed
color mapping. While this may appear purely beneficial to the user, the
software advantages are numerous as well. Imprecise rendering operations can
now generate small, nearly invisible errors instead of visibly incorrect
results through the use of anti-aliased drawing. With smooth gradations of
color available, there is no requirement for the system to support dithering
or other color-approximating schemes.

Finally, `Mado` assumes that the target machine provides respectable CPU
performance. This reduces the need to cache intermediate rendering results,
like glyph images for text. Having a uniformly performant target market means
that `Mado` needs to support only one general performance class of drawing
operations. For example, `Mado` supports only anti-aliased drawing;
non-anti-aliased drawing would be faster, but the CPUs supported by `Mado` are
required to be fast enough to make this irrelevant.

The combined effect of these environmental limitations means that `Mado` can
provide significant functionality with little wasted code. Window systems
designed for a range of target platforms must often generalize functionality
and expose applications to variability which will not, in practice, ever be
experienced by them. Eliminating choice has benefits beyond the mere reduction
of window system code; it reflects throughout the application stack.

## Build and Verify

`Mado` is built with a minimalist design in mind. However, its verification
relies on certain third-party packages for full functionality and access to all
its features. To ensure proper operation, the development environment should
have the [SDL2 library](https://www.libsdl.org/) and [libpng](https://github.com/pnggroup/libpng) installed.
* macOS: `brew install sdl2 libpng`
* Ubuntu Linux / Debian: `sudo apt install libsdl2-dev libpng-dev`

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

Mado is available under a MIT-style license, permitting liberal commercial use.
Use of this source code is governed by a MIT license that can be found in the [LICENSE](LICENSE) file.
