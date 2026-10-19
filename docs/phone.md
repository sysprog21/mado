# Mobile and Desktop Scenes

This document describes how Mado runs a phone user interface or a classic
desktop from the same core, and why the core is shaped the way it is.  The
cellphone demo under `apps/cellphone/` is the reference workload: a lock
screen, a home screen and twelve apps on a 240x320 display, drawn with
anti-aliased vector graphics (plus JPEG photo fixtures in Photos) on a CPU
without graphics acceleration.

## Window management is a policy

A window is a pixmap plus client callbacks.  Everything else (where the
window goes, whether it has a title bar, which window gets the pointer,
what happens when it closes) belongs to a *window manager policy* chosen
when the library is built:

| Policy | Behavior | Kconfig |
|--------|----------|---------|
| none (minimal) | No decorations; application windows cover the screen; the newest window, or the one last pressed, has the keyboard, and hiding it hands the keyboard to the window below. | `WM_NONE` |
| desktop (classic) | Overlapping decorated windows, click-to-raise, drag-to-move (the title bar always stays reachable), double-click the title to maximize, drop on a screen edge to snap, drag the bottom-right client corner to resize, iconify, close, shadow under the active window. New windows cascade off occupied spots. | `WM_DESKTOP` |
| mobile (embedded) | Application windows stack over the work area between optional system bars; only the top one is visible; animated push/pop/dismiss, an edge-swipe back gesture and Escape for back. | `WM_MOBILE` |

The Kconfig "Window manager" choice builds exactly one of them, and every
screen runs it for its whole life: there is no switching and no run-time
check of which policy is active.  Keeping one policy per build keeps the
others' code and state out of targets that never use them, and a product
ships as a phone, a desktop or a kiosk, not as all three.  Code and
fields only one policy needs are compiled out of the others: system bars
and the keyboard inset exist only in mobile builds, title bars and the
window title only in desktop builds, and drop shadows only with
`DROP_SHADOW`.

```c
twin_screen_set_bars(screen, status, nav);  /* mobile system bars */
twin_window_close(window);                  /* WM-aware close */
twin_window_back(window);                   /* in-app back, else close */
twin_window_activate(window);               /* raise and focus via the WM */
twin_screen_focus_next(screen);             /* cycle windows (Alt+Tab) */
twin_screen_set_inset(screen, kbd_height);  /* keep an on-screen keyboard clear */
```

Applications describe intent through the window style rather than
geometry: `TwinWindowPlain` (placed exactly as requested, never decorated),
`TwinWindowApplication` (a managed app window) and `TwinWindowFullscreen`
(above everything, e.g. a lock screen).  The same app code therefore runs
decorated on a desktop, stacked on a phone, or full screen with no window
manager at all.  The demo apps are the proof.  `apps/apps.c` registers
every app that is built, and which policies an app runs under is settled
at build time too: ordinary apps run under all three, while the phone
shell, which owns the screen, depends on `WM_MOBILE` in Kconfig and is
compiled in only there.  A desktop build opens the apps as overlapping
windows, the phone launcher lists them and the mobile policy stacks their
windows, and a `WM_NONE` build shows the first one full screen.
`test-demos` runs each app under the policy of the build.
`screen->windows` lists every window bottom to top and is the stacking
order for all policies.

The policy interface (`src/twin_private.h`) is a set of `_twin_wm_*`
functions that the built policy defines and generic code calls directly,
with no function-pointer table: init/fini, layout, map/unmap, dispatch,
refresh (re-place the stack after a style, opacity, system-bar, inset or
screen-size change), and, for the policies that need them, decorate, hit
test and activate (desktop) or a pointer filter for gestures and the work
area (mobile).  A hook a policy has no use for is an inline no-op, so the
call compiles away; outside the desktop, activation is the policy's map.
Policy state is allocated with the screen, so starting a policy cannot
fail.  The work area counts only mapped bars, minus the inset set with
`twin_screen_set_inset()`.
`twin_screen_resize()` (a rotated panel, a resized host window) re-lays out
every window: phone apps and bars refit, desktop windows come back on
screen and maximized ones refit.

The system bars are registered, not owned: a replaced bar is unmapped, a
destroyed bar unregisters itself, and resizing a bar re-lays out the
applications.

Back is a request, not a close: the edge swipe and Escape send
`TwinEventBack`, so an app with its own history steps back inside its
window (the swipe then springs home); unhandled, the window closes.  The
swipe only starts after the finger moves sideways past a few pixels, so a
tap or a vertical drag at the left edge still reaches the app.  A press
that lands during a transition is dropped together with its release, and
a push moves the keyboard to the incoming app at once.

Pointer grabs are explicit: the release goes to the window that took the
press, even when the pointer ends up over another one, and hover updates
re-check their targets after each Leave/Enter handler, so a window may
destroy itself from any of its own handlers.  Generic code never asks
which policy is running.  `src/wm.c` holds the screen-wide services;
`src/wm-none.c`, `src/wm-desktop.c` and `src/wm-mobile.c` are the
policies.  A screen being destroyed first lets its policy go (`fini`) and
then tears the windows down without it, so the policy never sees half a
teardown.

Policy state stays out of the compositor.  Iconifying, for instance, is the
desktop policy limiting how many rows of the window's pixmap are shown
(`scan_height`): the compositor, damage and hit testing honour that limit
without knowing why it is set, and the hidden rows keep their pixels for
the restore.

### Mobile transitions cost no repainting

A push slides the new app in from the right while the previous one drifts
left by a third and dims; a pop reverses it; a full-screen window (the lock
screen) slides up when dismissed.  None of this repaints an application:
the policy only moves retained pixmaps and changes their opacity, and the
compositor does the rest.  When an application destroys its window, the
policy adopts the window's pixmap and animates that, so application state
is freed immediately and never touched by the animation.

Every arrangement ends in `mobile_settle()`, which derives the visible set
from the window list alone.  Transitions only perturb positions and opacity
in between, so interrupting one (a tap, a second push, a resize) is
always safe: finish, settle, start the next.

### Memory: one surface regardless of stack depth

Only the top application is visible under the mobile policy, so the
pixels of every application below it are released (eviction, Kconfig
`WM_MOBILE_EVICT`).  The window keeps a storage-less stub pixmap whose
empty clip turns every drawing call into a no-op, accumulates damage, and
is repainted from application state when revealed.  An opaque full-screen
window (the lock screen) likewise releases the app and the bars under it.
Steady-state memory is one work-area surface plus the bars, independent of
how many screens are stacked.  Phone screens can also render into RGB565
surfaces (`DEMO_CELLPHONE_RGB16`) to halve that again.

Heap measured by the `test-cellphone` budget gate: everything allocated
through `twin_malloc` except the bounded reuse pools (the 24 KiB coverage
cache and the path, mask and span buffers).  The peak is the home figure
plus the allocator's growth while every app opens, so it includes the second
surface alive during transitions and decoded photos:

| Surfaces | At home | Peak across all twelve apps |
|----------|---------|-----------------------------|
| ARGB32 | 330 KB | 724 KB |
| RGB565 | 176 KB | 446 KB |

The trade-off is deliberate: a renderer that draws straight into partial
scan-out buffers needs less RAM, but must repaint every pixel of every
animation frame.  Retaining the visible screen lets push, pop, swipe and
scroll run on memmove and compositing alone, which is what keeps them at
display rate on a CPU without graphics hardware.

## Compositor

The screen never holds a full framebuffer.  Each update composes the
damaged region scanline by scanline from the window pixmaps into one span
buffer and hands it to the backend.

- **Damage is a short list.**  Up to eight rectangles per screen and four
  per window.  Two rectangles merge only when their union covers no more
  pixels than the pair, so a clock tick in the status bar and a progress
  bar in the app stay two small updates.  Windows paint each damaged
  rectangle under its own clip.
- **Occlusion culling.**  Walking layers top-down, every opaque layer adds
  its extent on the current row to a small set of covered intervals;
  layers below are composited only where still visible and the background
  only fills what nothing covers.  Opaque regions come from RGB565 pixmaps
  and windows that declare `twin_window_set_opaque()`.
- **Opacity.**  The window manager can give a pixmap a constant alpha
  applied at scan-out (fades, dimming during transitions).
- **Procedural shadows.**  The window manager can attach a shadow
  that is synthesized during scan-out from a separable smoothstep profile,
  as a pseudo-layer directly under its pixmap.  It costs four bytes per
  pixmap: no enlarged pixmap, no blur pass, no temporary copy.  The opaque
  pixmap above culls its interior automatically.
- **Scrolling.**  `twin_window_scroll()` moves retained pixels and damages
  only the exposed band.  Damage still pending inside the area moves with
  the pixels, so stale pixels are repainted where they land.

`twin_screen_stats()` counts frames, rectangles, pixels sent to the backend,
layer pixels composited, pixels skipped by occlusion and shadow pixels.
The counts are deterministic, which lets tests gate on work done.

## Vector graphics and caching

Anti-aliased rasterization (4x4 supersampled scan conversion) dominates the
cost of redrawing vector UI, and most of it repeats: a list, a keypad or a
home screen redraws the same few dozen shapes.

The **coverage cache** (`src/cache.c`) keeps A8 coverage masks in one LRU
with a byte budget (`CACHE_SIZE`, KiB).  Glyphs are keyed by font, size and
code point; filled paths by a hash of their device-space outline relative
to the pixel grid, so the same shape drawn at another integer offset hits.
Because the rasterizer's sample grid is pixel aligned, a cached mask is
bit-identical to a fresh one; the tests verify this.  Masks larger than
`CACHE_MAX_ENTRY` bytes are never admitted, and rasterization of large
paths is limited to the visible part of the clip.

**Text** (`twin_text_draw()`, `twin_text_width()`) composites cached glyph
masks with integer-snapped origins and 16.16 advances, and skips lines
outside the clip after measuring them.  Fonts are selectable per call.  Montserrat Medium and a 38-symbol
Font Awesome subset ship as TrueType outlines (`FONT_MONTSERRAT`,
`FONT_ICONS`, symbols as `TWIN_SYM_*` UTF-8 strings), generated by the
upgraded `tools/ttf/twin-ttf`.

**Gradients** (`twin_paint_path_linear()`) reuse the cached mask; vertical
gradients cost one solid span per scanline.

**Tweens** (`twin_tween_start()`) drive animation from elapsed time with
fixed-point easing curves, so a slow frame shortens an animation instead of
stretching it.  `twin_now()` is the shared clock; tests replace it.

Measured with `MADO_PERF_ONLY=vector ./mado-perf` on an x86-64 host with
the built-in renderer:

| Operation | Plain | Optimized | Speed-up |
|-----------|-------|-----------|----------|
| 20 glyphs at 14 px | 74.8 µs uncached | 10.2 µs cached | 7.3x |
| 44x44 rounded rectangle | 18.5 µs uncached | 4.6 µs cached | 4.0x |
| 240x320 scan-out, app over app plus status bar | 146.6 µs | 45.4 µs culled | 3.2x |

## API changes

Added: window manager policies and system bars, `TwinWindowFullscreen`,
`TwinEventClose` and `twin_window_close()`, `twin_window_set_opaque()`,
`twin_window_scroll()`, pixmap opacity and shadows, compositor statistics,
`twin_text_*`, `twin_paint_path_linear()`, tweens, `twin_now()` and
`twin_toplevel_destroy()`.

Removed or made internal: the compositor-level pixmap calls (show, hide,
move, opacity, shadow, update batching), the work queue, cursor setup
(`twin_create()` installs the arrow under `CONFIG_CURSOR`),
`twin_context_screen()` (`twin_run()` hands the callback the screen),
font measuring internals and the cache controls.  Simplified:
`twin_toplevel_create()` (always an ARGB32 application window),
`twin_pixmap_from_file()` (always ARGB32), `twin_tvg_to_pixmap()` (one
call), `twin_label_set()` (text only), custom widgets (the handler gets
the custom widget; no lookup), and `twin_window_damage()`, which queues
the repaint itself.

Removed or made internal, because nothing needed them or they encoded the
old fixed window manager: `twin_stack_blur()`, the pixmap-baked shadow
(`twin_shadow_border()`, `window->shadow_x/y`, the Gaussian LUT
generator), `twin_pixmap_transparent()`, `twin_pixmap_is_iconified()`,
`twin_screen_get_background()`, `twin_pixmap_create_budget()`,
`twin_custom_widget_base()`, the never-implemented
`twin_path_ucs4_stroke()`, joystick events, `CONFIG_WINDOW_MANAGER` and
`CONFIG_SHADOW_FADE_TAIL`.  Nothing in the tree called `twin_sin()`,
`twin_tan()`, `twin_atan2()`, `twin_path_rmove()`, `twin_path_rdraw()`,
`twin_path_identity()`, the path font getters, `twin_screen_get_active()`,
`twin_widget_create()`, toolkit keyboard focus
(`twin_widget_set_focusable()`), `TwinStyleBoldOblique`,
`twin_event_queue_t` or 13 of the icon symbols, so they are gone (the icon
font shrank with them).  Helpers only the library uses (matrix, path and
pixmap internals, `twin_icon_draw()`, the animation iterator) moved to
`src/twin_private.h`; an animated pixmap now owns its animation and frees
it in `twin_pixmap_destroy()`.  The box/label/button toolkit is now
optional (`WIDGETS`); the phone does not use it.

`include/twin.h` holds only the interface.  Screens, windows, pixmaps,
fonts, animations, widgets and the context are opaque; their layouts live
in `src/twin_private.h`, and applications use accessors instead
(`twin_window_pixmap()`, `twin_window_client()`, `twin_window_set_callbacks()`,
`twin_pixmap_width()`, `twin_pixmap_set_transform()`, `twin_screen_width()`,
`twin_toplevel_box()`, `twin_button_widget()` and similar).  The backend
interface (`twin_screen_create()`, `twin_screen_update()`,
`twin_screen_dispatch()` and the span callbacks) moved there too: only
backends, the tests and `mado-perf` drive a screen directly.  Without a
window manager, the newest mapped window now takes the keyboard.

Fixed along the way: TrueType glyphs were decoded with the wrong header
size and orientation, and the converter emitted cubic segments as
quadratics; RGB565 pixmaps were composited incorrectly (a macro evaluated
`*p++` several times); the closure tracker leaked an entry per distinct
closure until scheduling failed, and timeouts or work whose owner died were
retried forever; display-list insertion missed a back link; destroyed
windows could stay referenced as the active or pointer target; scaled
composites sampled the wrong source pixels when clipped; the widget toolkit
never freed widgets, custom-widget data or toplevels; the SDL backend
refused to start without an accelerated renderer and presented once per
damage rectangle; the headless backend recomposited on every damage call
and busy-waited; the fbdev and DRM backends no longer compiled with current
GCC; several left shifts of negative values; and `make check` ignored
header dependencies.

## The phone

`apps/cellphone/phone.h` is the whole contract for apps.  A screen is a
*view*: one window plus a C struct that embeds `ui_view_t` first.  Views
paint immediate-mode with the kit's painters (text, symbols, rounded
rectangles, gradients, list rows, buttons, sliders, switches, a soft
keyboard, text fields) and receive gesture events (press, drag with
velocity, release, tap, key).  There is no per-widget object tree; state
is plain fields in the view struct, which is why a covered screen can be
evicted and repainted at any time.  Timers and tweens created through the
view die with it.

`shell.c` owns the themes (Sage, Dark, Ocean), wallpapers, clock, app
registry with squircle icon plates, the status and navigation bars, and
navigation (`phone_open()`, `phone_back()`, `phone_home()`), which is
nothing more than showing and closing views: the window manager owns the
stack, the transitions and the memory.  Navigation acts on the topmost
application window whether or not it is a view, so a registry app opened
from the launcher goes back and home like any phone app.

### Visual language: Sage

The default theme, Sage, is a soft information-appliance style written
along the lines of the GNOME HIG: warm white pages, sage green grouping
and deep olive bars give a quiet reading surface, and rounded controls
with a few tactile launcher icons keep it friendly.  It is a custom design
language, not Adwaita.

- Three surfaces carry the structure, named in `phone_theme_t`: the page
  and its content (`bg`, `card`), grouped areas a step darker (`group`:
  section labels, the settings icon column, the navigation tray and the
  launcher behind its tiles), and the deep bars (`statusbar`, `header`)
  with `on_bar` text.  Spacing, tone and thin separators divide content;
  shadows only mark a control as raised.
- Colors keep one meaning: `primary` (olive) marks the selection and the
  main action, `call_red` a destructive one.  A state is never shown by
  color alone: the current navigation tab is also the filled key, a
  switch also moves its knob, a radio also fills its dot.
- Header bars place one start action (Contacts puts Add there), the title
  centered, and an end action or short status text; header buttons have
  no fill until pressed.  The title moves to the start only when the end
  text would collide with it.
- Buttons are soft rounded fills lit slightly from the top, with labels
  that name the action.  The navigation bar reaches the top-level pages
  (Dialer, Home, Contacts), so its keys pair an icon with a label;
  ordinary buttons carry one or the other.  Above home, its center key
  turns into Back.
- Settings apply immediately; there is no OK or Apply step.  Rows use one
  set of single-color symbols, while the launcher keeps small tactile
  icons, so decoration and operation stay apart.
- Dot texture appears only in empty header slots, never behind text, and
  typography stays a few sans-serif sizes, with capitals kept to
  acronyms such as the camera's HDR and REC.

Dark and Ocean fill the same roles, so the shell and every themed control
follow a theme switch without special cases.  The camera and the game
panels keep fixed palettes of their own.

The default build uses the desktop policy, so build the phone with the
mobile one, then run it with SDL (2x scaled by default; `MADO_SCALE=n`
overrides):

```shell
$ make defconfig
$ python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig WM_MOBILE=y
$ python3 tools/kconfig/genconfig.py configs/Kconfig
$ make
$ ./demo-sdl                      # the phone
```

The default build runs the desktop demos; a `WM_NONE=y` build shows a
single app with no window manager.

Drag the lock screen knob to unlock.  Drag from the left edge of an app, or
press Escape, to go back.  A hardware keyboard types into the focused
screen.  In the desktop scene Alt+Tab cycles the windows.

## Verification

`make check` builds and runs the headless suites in-process: scan-out
lands in a memory framebuffer, time is a virtual clock, input is injected
through `twin_screen_dispatch()`.  Mado is integer-only, so with the
built-in renderer every frame is bit-exact across hosts.

- `test-core`: damage merging, compositor paths, shadows, opacity, stub
  pixmaps, text, gradients, tweens, closure lifetime, scroll blitting,
  multi-rectangle window damage, public accessors, leaks.
- `test-wm`: placement and input under the built policy, desktop activation,
  focus after iconify/close, cycling, drag clamping, maximize, snap,
  resize and releases outside the grab, windows destroying themselves from
  their handlers, mobile push/pop/swipe/dismiss with mid-transition checks,
  edge taps, back handling, bars hidden/replaced/destroyed/resized, the
  keyboard inset, screen rotation and, with `WM_MOBILE_EVICT`, the memory
  of covered windows.
- `test-lifetime`: callbacks that run while an object is torn down, or
  that act on the object calling them.
- `test-demos`: every registry app except the phone shell (covered by
  `test-cellphone`) under the built policy: placement,
  consistency while it runs, repainting revealed windows, closing window
  by window, a start that runs out of memory, leaks.
- `test-cellphone` (mobile builds): boots the phone with a fixed clock and
  drives the whole user journey through every app, including the registry
  apps opened from the launcher.

The checks are layered:

- **Differential.**  Every optimization has a plain counterpart and must
  produce identical bytes: occlusion culling on and off, coverage cache
  cold, warm, disabled and starved, blit scrolling against a full repaint.
- **Damage consistency.**  `th_damage_check()` recomposites the whole
  screen from scratch with every optimization off and compares it with the
  incrementally maintained frame.  Any change that was not damaged shows up
  as stale pixels.  It runs after every navigation step and in the middle
  of transitions.
- **Golden frames.**  CRC-32 of key screens in `tests/golden/*.txt`,
  tagged with a fingerprint of the rendering-relevant build options and
  skipped when those differ.  `make check-update` records new ones;
  `MADO_SCREENSHOTS=dir` writes PPM screenshots of every golden frame.
- **Work and memory gates.**  Heap returns to its baseline after app
  cycles; covered screens cost no pixels; per-tick
  repaint area of animated screens is bounded through `twin_screen_stats()`.

CI runs the suites under the default configuration, Valgrind,
AddressSanitizer with UndefinedBehaviorSanitizer, and a configuration
matrix (no window manager, single policies, cache disabled, no widget
toolkit, RGB565 surfaces, TLSF heap, Pixman), plus an end-to-end run of
`demo-headless` driven through `headless-ctl` (now able to `type` text).
