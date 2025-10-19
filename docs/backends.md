# Graphics Backends for Mado

Mado supports multiple graphics backends to accommodate different deployment scenarios and requirements. Each backend provides the same API to applications, ensuring code portability across different display systems.

All backends maintain the same application interface, so switching between them requires only a recompilation, not code changes.

## Available Backends

### SDL Backend
The SDL (Simple DirectMedia Layer) backend provides cross-platform graphics output with hardware acceleration support.
It also serves as the only backend supporting WebAssembly/Emscripten builds for browser deployment.

Features:
- Cross-platform (Windows, macOS, Linux, etc.)
- WebAssembly/Emscripten support for browser deployment
- Hardware-accelerated rendering when available
- Full keyboard and mouse input support
- Windowed and fullscreen modes
- Audio support (if needed)

#### Native Builds

Dependencies:
Install the [SDL2 library](https://www.libsdl.org/):
* macOS: `brew install sdl2`
* Ubuntu Linux / Debian: `sudo apt install libsdl2-dev`

Build:
```shell
make defconfig  # SDL backend is enabled by default
make
```

Run:
```shell
./demo-sdl
```

Once the window appears, you can move windows and interact with widgets.

#### WebAssembly/Emscripten Builds

The SDL backend can be compiled to WebAssembly using Emscripten, enabling Mado applications to run in web browsers.

Dependencies:
Install the [Emscripten SDK](https://emscripten.org/):
```shell
# Clone the Emscripten SDK
git clone https://github.com/emscripten-core/emsdk
cd emsdk

# Install and activate the latest version
./emsdk install latest
./emsdk activate latest

# Add Emscripten to your PATH (add to ~/.bashrc or ~/.zshrc)
source ./emsdk_env.sh
```

Build:
```shell
# The build system auto-detects Emscripten when CC=emcc
make defconfig
make CC=emcc
```

The build system automatically:
- Detects Emscripten compiler via `scripts/detect-compiler.py`
- Restricts backend selection to SDL only (VNC/fbdev incompatible with WebAssembly)
- Disables incompatible features (Cairo-based font editor)
- Uses Emscripten's SDL2 port (no manual installation needed)
- Copies build artifacts to `assets/web/` directory

Run:
```shell
# Start the development server
./scripts/serve-wasm.py --open

# Or manually:
python3 scripts/serve-wasm.py
# Then open http://localhost:8080 in your browser
```

The web interface (`assets/web/index.html`) provides:
- Canvas-based rendering with SDL2
- Keyboard and mouse event handling
- Console output for debugging
- Error reporting and status display
- Automatic module loading and initialization

Browser Requirements:
- Modern browser with WebAssembly support (Chrome 57+, Firefox 52+, Safari 11+, Edge 16+)
- JavaScript enabled
- Hardware acceleration recommended for better performance

Known Limitations:
- Single-threaded execution (no WebWorkers support yet)
- No direct file system access (use Emscripten's virtual filesystem)
- Initial load time proportional to binary size

Use Cases:
- Desktop applications
- Cross-platform development
- Primary development and testing platform
- Web-based demonstrations and interactive tutorials
- Browser-based UI prototyping
- Client-side graphics applications without installation

### Linux Framebuffer (fbdev)
Direct framebuffer access for embedded Linux systems without X11/Wayland.

Features:
- Direct hardware access
- Minimal dependencies
- Built-in cursor support
- Linux input subsystem integration
- Virtual terminal switching support

Dependencies:
Install `libudev` and `libuuid`:
* Ubuntu Linux / Debian: `sudo apt install libudev-dev uuid-dev`

Build:
```shell
make defconfig
python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig \
    BACKEND_FBDEV=y \
    BACKEND_SDL=n
make
```

Run:
```shell
sudo ./demo-fbdev
```

Normal users don't have access to `/dev/fb0`, requiring `sudo`. Alternatively, add the user to the video group:
```shell
sudo usermod -a -G video $USERNAME
```

The framebuffer device can be assigned via the environment variable `FRAMEBUFFER`.

Use Cases:
- Embedded Linux systems
- Kiosk applications
- Boot splash screens
- Lightweight desktop environments

### VNC Backend
Provides remote display capabilities through the VNC (Virtual Network Computing) protocol.

Features:
- Remote access over network
- Multiple client support
- Platform-independent clients
- Built-in authentication
- Compression support

Dependencies:
Install [neatvnc](https://github.com/any1/neatvnc). Note: The VNC backend has only been tested on GNU/Linux, and prebuilt packages might be outdated. To ensure the latest version, build from source:
```shell
tools/build-neatvnc.sh
```

Build:
```shell
make defconfig
python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig \
    BACKEND_VNC=y \
    BACKEND_SDL=n
make
```

Run:
```shell
./demo-vnc
```

This starts the VNC server. Connect using any VNC client with the specified IP address (default: `127.0.0.1`) and port (default: `5900`).
- IP address: Set via `MADO_VNC_HOST` environment variable
- Port: Set via `MADO_VNC_PORT` environment variable

Use Cases:
- Remote desktop applications
- Server-side rendering
- Thin client deployments
- Remote administration tools

### Headless Backend
Renders to a memory buffer without any display output, ideal for testing and automation.

Features:
- No display dependencies
- Shared memory architecture
- Screenshot capability
- Event injection for testing
- Memory debugging friendly

Dependencies:
No additional dependencies required. This backend uses shared memory for rendering.

Build:
```shell
# Using setconfig.py (recommended)
make defconfig
python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig \
    BACKEND_HEADLESS=y \
    BACKEND_SDL=n \
    TOOLS=y \
    TOOL_HEADLESS_CTL=y
make

# Alternative: Interactive configuration
make config  # Select "Headless backend" and "Headless control tool"
make
```

Run:
```shell
./demo-headless &
./headless-ctl status           # Check backend status
./headless-ctl shot output.png  # Capture screenshot
./headless-ctl shutdown         # Graceful shutdown
```

The headless backend uses shared memory for rendering without display output. Use `headless-ctl` to monitor, control, and capture screenshots.

Use Cases:
- Automated testing
- CI/CD pipelines
- Memory analysis with Valgrind or AddressSanitizer
- Screenshot generation
- Performance benchmarking

## Backend Selection

Backends are selected at compile time through the Kconfig-based build system.

WebAssembly Compatibility Note:
When compiling with Emscripten (`CC=emcc`), the build system automatically restricts backend selection to SDL only.
The following backends are incompatible with WebAssembly:
- VNC Backend: Requires native networking APIs not available in WebAssembly
- Framebuffer Backend: Requires direct Linux kernel interfaces (`/dev/fb0`, ioctls)
- Headless Backend: Requires POSIX shared memory APIs (`shm_open`, `mmap`)

The Kconfig system automatically enforces these restrictions when Emscripten is detected.

### Configuration Methods

Mado uses [Kconfiglib](https://github.com/sysprog21/Kconfiglib) for configuration management, providing several methods to configure backends:

Method 1: Interactive Configuration (best for exploration)
```shell
make config          # Terminal-based menu
# or
make menuconfig      # Alternative terminal interface
make
```

Method 2: Using `setconfig.py` (best for scripting)
```shell
make defconfig
python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig \
    BACKEND_SDL=y        # or BACKEND_FBDEV=y, BACKEND_VNC=y, etc.
make
```

Advantages:
- Single command to set multiple options
- Automatically handles dependencies
- No need to manually edit `.config`
- Symbol names don't require `CONFIG_` prefix

Method 3: Configuration Fragments (best for CI/CD)
```shell
# Create a fragment file (e.g., configs/mybackend.fragment)
# Contents: CONFIG_BACKEND_SDL=y
#           CONFIG_TOOLS=y

make defconfig
python3 tools/kconfig/examples/merge_config.py configs/Kconfig .config \
    configs/defconfig configs/mybackend.fragment
make
```

Advantages:
- Reusable configuration snippets
- Version controllable
- Easy to share and document
- Handles dependency conflicts automatically

## Common Backend Interface

All backends implement the same core interface:
- Screen initialization and configuration
- Pixel buffer management
- Event handling (mouse, keyboard)
- Screen updates and synchronization
- Cleanup and resource management

This ensures applications written for Mado work seamlessly across all backends without modification.

## Headless Backend Details

The headless backend deserves special attention as it enables unique testing and automation capabilities.

### Architecture

The headless backend uses POSIX shared memory for inter-process communication:
- Server: The Mado application with headless backend creates and manages the shared memory
- Client: The `headless-ctl` tool connects to the shared memory to control and monitor the backend

### Headless Control Tool (headless-ctl)

The `headless-ctl` tool provides command-line access to the headless backend:

#### Commands

- `status`: Get backend status with detailed statistics
  - Screen dimensions
  - Rendering performance (FPS, frame count)
  - Event statistics
  - Last activity timestamp
- `shot FILE.png`: Save current framebuffer to FILE.png
  - Supports PNG format with minimal dependencies
  - No external image libraries required
- `shutdown`: Gracefully shutdown the backend
- `mouse TYPE X Y [BUTTON]`: Inject mouse events
  - TYPE: move, down, up
  - X, Y: Screen coordinates (validated against screen size)
  - BUTTON: Button number (optional, default 0)
- `monitor`: Live monitoring of backend activity
  - Real-time FPS display
  - Frame rendering statistics
  - Event processing counts
  - Backend health status

#### Examples

```shell
# Check backend status
$ ./headless-ctl status
events=42 frames=1250 fps=60 running=1
Screen: 640x480
Rendering: 60 FPS, 1250 frames total
Events: 42 total (0/sec, 12 mouse)
Commands: 1
Last Activity: 14:23:45.123456
Status: Active (12.3ms ago)

# Take a screenshot
$ ./headless-ctl shot screenshot.png
Screenshot data available in framebuffer
Screenshot saved to screenshot.png

# Simulate mouse click at (100, 200)
./headless-ctl mouse move 100 200
./headless-ctl mouse down 100 200 1
./headless-ctl mouse up 100 200 1

# Monitor backend activity in real-time
$ ./headless-ctl monitor
Monitoring backend activity (Ctrl+C to stop)...
Time             Events  Frames   FPS  Status
---------------  ------  ------  ----  --------
14:23:45.123456      42    1250    60  ACTIVE
14:23:46.234567      42    1310    60  IDLE
14:23:47.345678      45    1370    60  ACTIVE
```

### Memory Debugging

The headless backend is ideal for memory analysis:

```shell
# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./demo-headless &

# In another terminal, interact with the backend
./headless-ctl mouse move 100 100
./headless-ctl shot test.png
./headless-ctl shutdown

# Run with AddressSanitizer
make defconfig
echo "CONFIG_BACKEND_HEADLESS=y" >> .config
python3 tools/kconfig/genconfig.py configs/Kconfig
make CFLAGS="-fsanitize=address -g"
./demo-headless
```

### Limitations

- Single shared memory segment (one backend instance at a time)
- No hardware acceleration (software rendering only)
- PNG output format uses uncompressed DEFLATE for simplicity
- Maximum 64 queued events in circular buffer
- Memory usage doubled due to double buffering (front + back buffers)
- Coordinate validation enforced - events outside screen bounds are rejected
- Command timeout set to 3 seconds for control tool operations

## Backend-Specific Configuration

Each backend may have specific configuration options:

### SDL Backend

Native Configuration:
- Window size and position
- Fullscreen mode toggle
- Hardware acceleration preferences
- VSync settings

WebAssembly Configuration:
- Canvas element ID (default: `#canvas`, configured in `assets/web/index.html`)
- Keyboard input element (set via `SDL_EMSCRIPTEN_KEYBOARD_ELEMENT`)
- Initial canvas size (640x480 default, configurable in HTML)
- Browser console logging (enabled by default for debugging)
- Main loop timing (managed by `emscripten_set_main_loop_arg`)
- Memory allocation limits (configurable via Emscripten linker flags)

### Framebuffer Backend
- Device path (default: `/dev/fb0`)
- Input device configuration
- Cursor settings
- Color depth adaptation

### VNC Backend
- Port number (default: 5900)
- Authentication methods
- Compression levels
- Client connection limits

### Headless Backend
- Buffer dimensions
- Shared memory path
- Event queue size
- Screenshot triggers
