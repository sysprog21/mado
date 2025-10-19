#!/usr/bin/env python3
"""
WebAssembly Development Server for Mado
Serves the web/ directory and provides instructions for testing WebAssembly builds.
"""

import http.server
import socketserver
import os
import sys
import argparse
import webbrowser
import subprocess
import signal
from pathlib import Path

# ANSI color codes
BOLD = '\033[1m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
RED = '\033[91m'
RESET = '\033[0m'

def find_process_using_port(port):
    """Find process ID using the given port."""
    try:
        # macOS/BSD: use lsof
        result = subprocess.run(
            ['lsof', '-ti', f'tcp:{port}'],
            capture_output=True,
            text=True
        )
        if result.returncode == 0 and result.stdout.strip():
            pids = result.stdout.strip().split('\n')
            return [int(pid) for pid in pids if pid]
    except (subprocess.SubprocessError, FileNotFoundError):
        # Linux: use ss or netstat as fallback
        try:
            result = subprocess.run(
                ['ss', '-tlnp', f'sport = :{port}'],
                capture_output=True,
                text=True
            )
            # Parse ss output for PID
            for line in result.stdout.split('\n'):
                if 'pid=' in line:
                    pid_part = line.split('pid=')[1].split(',')[0]
                    return [int(pid_part)]
        except (subprocess.SubprocessError, FileNotFoundError):
            pass
    return []

def kill_process(pid):
    """Kill process by PID."""
    try:
        os.kill(pid, signal.SIGTERM)
        return True
    except (ProcessLookupError, PermissionError):
        return False

def check_required_files(web_dir):
    """Check if required WebAssembly files exist."""
    required_files = [
        'index.html',
        'demo-wasm.wasm',
        'demo-wasm.js',  # Emscripten JS glue code
        'mado-wasm.js'
    ]

    missing = []
    for filename in required_files:
        filepath = web_dir / filename
        if not filepath.exists():
            missing.append(filename)

    return missing

def print_banner(port, web_dir, open_browser):
    """Print informative banner with instructions."""
    print(f"\n{BOLD}{GREEN}{'='*70}{RESET}")
    print(f"{BOLD}{GREEN}  Mado WebAssembly Development Server{RESET}")
    print(f"{BOLD}{GREEN}{'='*70}{RESET}\n")

    print(f"{BOLD}Serving directory:{RESET} {web_dir}")
    print(f"{BOLD}Server address:{RESET}    http://localhost:{port}")
    print(f"{BOLD}Direct URL:{RESET}        http://localhost:{port}/index.html\n")

    if open_browser:
        print(f"{YELLOW}→ Opening browser automatically...{RESET}\n")
    else:
        print(f"{YELLOW}→ Open the URL above in your browser to test the WebAssembly build{RESET}\n")

    print(f"{BOLD}Instructions:{RESET}")
    print(f"  1. Make sure you have built the WebAssembly version:")
    print(f"     {BLUE}env CC=emcc make{RESET}")
    print(f"  2. Build artifacts are automatically copied to assets/web/")
    print(f"  3. Open the URL above in a modern browser\n")

    print(f"{BOLD}Supported browsers:{RESET}")
    print(f"  • Chrome/Chromium (recommended)")
    print(f"  • Firefox")
    print(f"  • Safari (macOS 11.3+)\n")

    print(f"{RED}Press Ctrl+C to stop the server{RESET}")
    print(f"{BOLD}{GREEN}{'='*70}{RESET}\n")

def main():
    parser = argparse.ArgumentParser(
        description='Serve Mado WebAssembly build for testing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s                    # Serve on default port 8000
  %(prog)s -p 8080            # Serve on port 8080
  %(prog)s --open             # Serve and open browser automatically
  %(prog)s -p 3000 --open     # Custom port with auto-open
        '''
    )

    parser.add_argument(
        '-p', '--port',
        type=int,
        default=8000,
        help='Port to serve on (default: 8000)'
    )

    parser.add_argument(
        '--open',
        action='store_true',
        help='Automatically open browser'
    )

    parser.add_argument(
        '--host',
        default='127.0.0.1',
        help='Host to bind to (default: 127.0.0.1)'
    )

    args = parser.parse_args()

    # Determine project root and web directory
    script_dir = Path(__file__).parent.absolute()
    project_root = script_dir.parent
    web_dir = project_root / 'assets' / 'web'

    # Check if web directory exists
    if not web_dir.exists():
        print(f"{RED}Error: assets/web/ directory not found at {web_dir}{RESET}", file=sys.stderr)
        print(f"{YELLOW}Creating assets/web/ directory...{RESET}")
        web_dir.mkdir(parents=True, exist_ok=True)
        print(f"{GREEN}Created: {web_dir}{RESET}")

    # Check for required files
    missing_files = check_required_files(web_dir)
    if missing_files:
        print(f"{YELLOW}Warning: Some required files are missing:{RESET}")
        for filename in missing_files:
            print(f"  {RED}✗{RESET} {filename}")
        print(f"\n{YELLOW}Build the WebAssembly version first:{RESET}")
        print(f"  {BLUE}env CC=emcc make defconfig{RESET}")
        print(f"  {BLUE}env CC=emcc make{RESET}")
        print(f"\n{YELLOW}Build artifacts will be automatically copied to assets/web/{RESET}\n")

        response = input(f"Continue anyway? [y/N]: ").strip().lower()
        if response not in ['y', 'yes']:
            print(f"{RED}Aborted.{RESET}")
            sys.exit(1)
        print()
    else:
        print(f"{GREEN}✓ All required files found{RESET}\n")

    # Change to web directory
    os.chdir(web_dir)

    # Print banner
    print_banner(args.port, web_dir, args.open)

    # Open browser if requested
    if args.open:
        url = f"http://{args.host}:{args.port}/index.html"
        webbrowser.open(url)

    # Start HTTP server
    Handler = http.server.SimpleHTTPRequestHandler

    # Add custom headers for COOP/COEP if needed for SharedArrayBuffer
    class CustomHandler(Handler):
        def end_headers(self):
            # Enable SharedArrayBuffer support (required for some Emscripten features)
            self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
            self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
            super().end_headers()

    # Enable socket reuse to allow quick restart
    socketserver.TCPServer.allow_reuse_address = True

    try:
        with socketserver.TCPServer((args.host, args.port), CustomHandler) as httpd:
            print(f"{GREEN}Server running...{RESET}\n")
            httpd.serve_forever()
    except KeyboardInterrupt:
        print(f"\n\n{YELLOW}Shutting down server...{RESET}")
        print(f"{GREEN}Server stopped.{RESET}\n")
        sys.exit(0)
    except OSError as e:
        if e.errno == 48 or e.errno == 98:  # Address already in use (macOS: 48, Linux: 98)
            print(f"\n{RED}Error: Port {args.port} is already in use{RESET}", file=sys.stderr)

            # Try to find the process using the port
            pids = find_process_using_port(args.port)

            if pids:
                print(f"{YELLOW}Found process(es) using port {args.port}: {pids}{RESET}")
                print(f"{YELLOW}This might be a previous instance of this server.{RESET}\n")

                response = input(f"Kill the process(es) and restart? [y/N]: ").strip().lower()
                if response in ['y', 'yes']:
                    killed = []
                    for pid in pids:
                        if kill_process(pid):
                            killed.append(pid)
                            print(f"{GREEN}✓ Killed process {pid}{RESET}")
                        else:
                            print(f"{RED}✗ Failed to kill process {pid}{RESET}")

                    if killed:
                        import time
                        print(f"{YELLOW}Waiting for port to be released...{RESET}")
                        time.sleep(1)

                        # Retry starting server
                        try:
                            with socketserver.TCPServer((args.host, args.port), CustomHandler) as httpd:
                                print_banner(args.port, web_dir, False)
                                if args.open:
                                    url = f"http://{args.host}:{args.port}/index.html"
                                    webbrowser.open(url)
                                print(f"{GREEN}Server running...{RESET}\n")
                                httpd.serve_forever()
                        except OSError:
                            print(f"{RED}Still cannot bind to port {args.port}{RESET}", file=sys.stderr)
                            print(f"{YELLOW}Try manually: {BLUE}lsof -ti tcp:{args.port} | xargs kill{RESET}\n", file=sys.stderr)
                            sys.exit(1)
                    else:
                        sys.exit(1)
                else:
                    print(f"{YELLOW}Try a different port with: {BLUE}--port XXXX{RESET}\n", file=sys.stderr)
                    sys.exit(1)
            else:
                print(f"{YELLOW}Could not identify the process using the port.{RESET}")
                print(f"{YELLOW}Try manually: {BLUE}lsof -ti tcp:{args.port} | xargs kill{RESET}")
                print(f"{YELLOW}Or use a different port with: {BLUE}--port XXXX{RESET}\n", file=sys.stderr)
                sys.exit(1)
        else:
            raise

if __name__ == '__main__':
    main()
