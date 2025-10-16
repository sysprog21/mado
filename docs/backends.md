# Graphics Backends for Mado

Mado supports multiple graphics backends to accommodate different deployment scenarios and requirements. Each backend provides the same API to applications, ensuring code portability across different display systems.

All backends maintain the same application interface, so switching between them requires only a recompilation, not code changes.

## Available Backends

### SDL Backend
The SDL (Simple DirectMedia Layer) backend provides cross-platform graphics output with hardware acceleration support.

**Features:**
- Cross-platform (Windows, macOS, Linux, etc.)
- Hardware-accelerated rendering when available
- Full keyboard and mouse input support
- Windowed and fullscreen modes
- Audio support (if needed)

**Build:**
```shell
make BACKEND=sdl
# or
make config  # Select "SDL video output support"
```

**Use Cases:**
- Desktop applications
- Cross-platform development
- Games and multimedia applications

### Linux Framebuffer (fbdev)
Direct framebuffer access for embedded Linux systems without X11/Wayland.

**Features:**
- Direct hardware access
- Minimal dependencies
- Built-in cursor support
- Linux input subsystem integration
- Virtual terminal switching support

**Build:**
```shell
make BACKEND=fbdev
# or
make config  # Select "Linux framebuffer support"
```

**Use Cases:**
- Embedded Linux systems
- Kiosk applications
- Boot splash screens
- Lightweight desktop environments

### VNC Backend
Provides remote display capabilities through the VNC (Virtual Network Computing) protocol.

**Features:**
- Remote access over network
- Multiple client support
- Platform-independent clients
- Built-in authentication
- Compression support

**Build:**
```shell
make BACKEND=vnc
# or
make config  # Select "VNC server output support"
```

**Use Cases:**
- Remote desktop applications
- Server-side rendering
- Thin client deployments
- Remote administration tools

### Headless Backend
Renders to a memory buffer without any display output, ideal for testing and automation.

**Features:**
- No display dependencies
- Shared memory architecture
- Screenshot capability
- Event injection for testing
- Memory debugging friendly

**Build:**
```shell
# Using setconfig.py (recommended)
make defconfig
python3 tools/kconfig/setconfig.py --kconfig configs/Kconfig \
    BACKEND_HEADLESS=y \
    TOOLS=y \
    TOOL_HEADLESS_CTL=y
make

# Alternative: Interactive configuration
make config  # Select "Headless backend" and "Headless control tool"
make
```

**Use Cases:**
- Automated testing
- CI/CD pipelines
- Memory analysis
- Screenshot generation
- Performance benchmarking

## Backend Selection

Backends are selected at compile time through the Kconfig-based build system.

### Configuration Methods

Mado uses [Kconfiglib](https://github.com/sysprog21/Kconfiglib) for configuration management, providing several methods to configure backends:

**Method 1: Interactive Configuration** (best for exploration)
```shell
make config          # Terminal-based menu
# or
make menuconfig      # Alternative terminal interface
make
```

**Method 2: Using `setconfig.py`** (best for scripting)
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

**Method 3: Configuration Fragments** (best for CI/CD)
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
- Window size and position
- Fullscreen mode toggle
- Hardware acceleration preferences
- VSync settings

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
