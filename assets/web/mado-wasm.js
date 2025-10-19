/*
 * Mado WebAssembly JavaScript Glue Code
 * Copyright (c) 2025 National Cheng Kung University, Taiwan
 * All rights reserved.
 */

/* Canvas rendering module
 * Handles framebuffer updates and Canvas 2D API interaction
 */
const MadoCanvas = {
  canvas: null,
  ctx: null,
  imageData: null,
  width: 0,
  height: 0,
  /* Cached framebuffer pointer for direct memory access */
  fbPtr: 0,
  fbBuffer: null,

  /* Initialize Canvas context */
  init: function (width, height) {
    this.canvas = document.getElementById("canvas");
    if (!this.canvas) {
      console.error("Canvas element not found");
      return false;
    }

    this.width = width;
    this.height = height;
    this.canvas.width = width;
    this.canvas.height = height;

    this.ctx = this.canvas.getContext("2d");
    this.imageData = this.ctx.createImageData(width, height);

    console.log(`Canvas initialized: ${width}x${height}`);
    return true;
  },

  /* Cache framebuffer pointer from WASM
   * Called once after WASM module initialization to avoid repeated
   * parameter passing through EM_ASM on every frame.
   */
  cacheFramebuffer: function () {
    if (typeof Module._mado_wasm_get_framebuffer === "undefined") {
      console.error("Framebuffer accessor functions not found");
      return false;
    }

    this.fbPtr = Module._mado_wasm_get_framebuffer();
    this.width = Module._mado_wasm_get_width();
    this.height = Module._mado_wasm_get_height();

    if (this.fbPtr === 0) {
      console.error("Invalid framebuffer pointer");
      return false;
    }

    /* Access WASM memory through Emscripten runtime */
    let wasmMemory = Module.HEAPU32 || (Module.HEAP32 && new Uint32Array(Module.HEAP32.buffer));

    if (!wasmMemory) {
      console.error("WASM memory not accessible");
      return false;
    }

    /* Cache the HEAPU32 view for faster access */
    const pixelCount = this.width * this.height;
    this.fbBuffer = new Uint32Array(
      wasmMemory.buffer,
      this.fbPtr,
      pixelCount
    );

    console.log(`Framebuffer cached: ${this.fbPtr} (${this.width}x${this.height})`);
    return true;
  },

  /* Update Canvas from framebuffer
   * Optimized version using cached framebuffer pointer.
   * No parameters needed - uses cached fbBuffer directly.
   */
  updateCanvas: function () {
    /* Lazy initialization: cache framebuffer on first call */
    if (!this.fbBuffer && !this.cacheFramebuffer()) {
      console.warn("Canvas or framebuffer not initialized");
      return;
    }

    if (!this.ctx || !this.imageData) {
      console.warn("Canvas not initialized");
      return;
    }

    /* Convert ARGB32 to RGBA for Canvas ImageData
     * Uses cached fbBuffer instead of accessing memory every frame
     */
    const data = this.imageData.data;
    const pixelCount = this.width * this.height;

    for (let i = 0; i < pixelCount; i++) {
      const argb = this.fbBuffer[i];
      const offset = i * 4;

      /* Extract components: ARGB -> RGBA */
      data[offset + 0] = (argb >> 16) & 0xff; // R
      data[offset + 1] = (argb >> 8) & 0xff;  // G
      data[offset + 2] = argb & 0xff;         // B
      data[offset + 3] = (argb >> 24) & 0xff; // A
    }

    /* Draw to Canvas */
    this.ctx.putImageData(this.imageData, 0, 0);
  },

  /* Cleanup */
  destroy: function () {
    this.canvas = null;
    this.ctx = null;
    this.imageData = null;
  },
};

/* Event handling module
 * Maps browser events to Mado C functions
 */
const MadoEvents = {
  /* Mouse button mapping: browser button -> Mado button */
  getButton: function (browserButton) {
    switch (browserButton) {
      case 0:
        return 0; // Left
      case 1:
        return 2; // Middle
      case 2:
        return 1; // Right
      default:
        return 0;
    }
  },

  /* Get button state bitmask
   * Browser e.buttons: 1=left, 2=right, 4=middle
   * Mado expects: 1=left, 2=middle, 4=right
   */
  getButtonState: function (buttons) {
    let state = 0;
    if (buttons & 1) state |= 1; // Left → Left
    if (buttons & 2) state |= 4; // Right → Right (swap: browser bit 2 → Mado bit 4)
    if (buttons & 4) state |= 2; // Middle → Middle (swap: browser bit 4 → Mado bit 2)
    return state;
  },

  /* Setup event listeners */
  init: function (canvas) {
    /* Mouse motion */
    canvas.addEventListener("mousemove", function (e) {
      const rect = canvas.getBoundingClientRect();
      const x = Math.floor(e.clientX - rect.left);
      const y = Math.floor(e.clientY - rect.top);
      const buttonState = MadoEvents.getButtonState(e.buttons);
      Module.ccall("mado_wasm_mouse_motion", null, ["number", "number", "number"], [x, y, buttonState]);
    });

    /* Mouse button down */
    canvas.addEventListener("mousedown", function (e) {
      e.preventDefault();
      const rect = canvas.getBoundingClientRect();
      const x = Math.floor(e.clientX - rect.left);
      const y = Math.floor(e.clientY - rect.top);
      const button = MadoEvents.getButton(e.button);
      Module.ccall("mado_wasm_mouse_button", null, ["number", "number", "number", "boolean"], [x, y, button, true]);
    });

    /* Mouse button up */
    canvas.addEventListener("mouseup", function (e) {
      e.preventDefault();
      const rect = canvas.getBoundingClientRect();
      const x = Math.floor(e.clientX - rect.left);
      const y = Math.floor(e.clientY - rect.top);
      const button = MadoEvents.getButton(e.button);
      Module.ccall("mado_wasm_mouse_button", null, ["number", "number", "number", "boolean"], [x, y, button, false]);
    });

    /* Keyboard events
     * Note: Direct keycode mapping. For production, consider:
     * - Key to SDL keycode translation table
     * - Handling special keys (arrows, function keys, etc.)
     */
    document.addEventListener("keydown", function (e) {
      // Prevent default browser shortcuts
      if (e.ctrlKey || e.metaKey || e.altKey) {
        e.preventDefault();
      }
      const keycode = e.keyCode || e.which;
      Module.ccall("mado_wasm_key", null, ["number", "boolean"], [keycode, true]);
    });

    document.addEventListener("keyup", function (e) {
      const keycode = e.keyCode || e.which;
      Module.ccall("mado_wasm_key", null, ["number", "boolean"], [keycode, false]);
    });

    /* Prevent context menu on canvas */
    canvas.addEventListener("contextmenu", function (e) {
      e.preventDefault();
    });

    console.log("Event handlers initialized");
  },
};

/* Emscripten Module configuration
 * Use Module = Module || {} to merge with Emscripten's generated config
 */
var Module = Module || {};

/* Tell Emscripten where to find the WASM file */
Module.locateFile = function (path, prefix) {
  if (path.endsWith(".wasm")) {
    return "demo-wasm.wasm";
  }
  return prefix + path;
};

/* Called when WebAssembly module is ready */
Module.onRuntimeInitialized = function () {
  console.log("Mado WebAssembly runtime initialized");

  /* Note: Framebuffer caching is done lazily on first updateCanvas() call
   * because twin_wasm_init() hasn't executed yet at this point
   */

  /* Setup event handlers */
  const canvas = document.getElementById("canvas");
  if (canvas) {
    MadoEvents.init(canvas);
  } else {
    console.error("Canvas element not found for event handling");
  }

  /* Note: Main loop is started by C code via emscripten_set_main_loop_arg
   * No need to call anything here - the backend initialization triggers it
   */
};

/* Print output to console */
Module.print = function (text) {
  console.log(text);
};

/* Print errors to console */
Module.printErr = function (text) {
  console.error(text);
};

/* Canvas element (required by Emscripten) */
Module.canvas = (function () {
  return document.getElementById("canvas");
})();

/* Utility: Load image using browser's Image API
 * This can be called from C via EM_ASM to decode JPEG/PNG
 * Returns ImageData that can be copied to twin_pixmap_t
 */
function madoLoadImage(url, callback) {
  const img = new Image();
  img.crossOrigin = "anonymous";

  img.onload = function () {
    const canvas = document.createElement("canvas");
    canvas.width = img.width;
    canvas.height = img.height;

    const ctx = canvas.getContext("2d");
    ctx.drawImage(img, 0, 0);

    const imageData = ctx.getImageData(0, 0, img.width, img.height);
    callback(imageData);
  };

  img.onerror = function () {
    console.error("Failed to load image:", url);
    callback(null);
  };

  img.src = url;
}
