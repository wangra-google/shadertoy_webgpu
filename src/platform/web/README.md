# Web Platform Module (`src/platform/web`)

This directory contains the web-specific platform implementation for the **Shadertoy WebGPU** application. It enables the C++ engine to be compiled to WebAssembly (WASM) using Emscripten, interfaces with the browser's WebGPU API, and provides a modern web interface.

## Directory Purpose

The `src/platform/web` module serves as the bridge between the C++ engine core (`src/core`) and the web browser. It implements the web-specific graphics context and exports the engine's API to JavaScript. This allows the application to run at native performance in the browser while providing an interactive web playground.

Key components in this module include:
- **WebGPU Context Setup**: Native WebGPU integration with the HTML5 `<canvas>`.
- **Emscripten Bindings**: Exporting the C++ engine API to JavaScript via Embind.
- **High-Performance Data Transfer**: Direct memory copying from JavaScript TypedArrays to C++ memory.
- **Web UI & Render Loop**: A premium front-end playground with interactive controls and a performance statistics overlay.

---

## File Overview

### [bindings.cpp](file:///C:/src/shadertoy_webgpu/src/platform/web/bindings.cpp)
Implements the JavaScript-to-C++ bindings using Emscripten's Embind (`EMSCRIPTEN_BINDINGS`).
- Defines the [EngineCore](file:///C:/src/shadertoy_webgpu/src/platform/web/bindings.cpp#L13) helper class, which orchestrates the [WebContext](file:///C:/src/shadertoy_webgpu/src/platform/web/web_context.h#L12) and [Renderer](file:///C:/src/shadertoy_webgpu/src/core/renderer.h#L43).
- Binds the `EngineCore` class to JavaScript, exposing methods to initialize the engine, tick the render loop, resize the viewport, swap geometries, and update uniforms.
- Implements [LoadTextureData](file:///C:/src/shadertoy_webgpu/src/platform/web/bindings.cpp#L79), which uses inline Emscripten assembly (`MAIN_THREAD_EM_ASM`) to copy pixel data directly from JavaScript TypedArrays into C++ vectors without unnecessary overhead.
- Supports passing JavaScript callback functions into the asynchronous shader compilation pipeline.

### [index.html](file:///C:/src/shadertoy_webgpu/src/platform/web/index.html)
The main entry point and user interface for the web application.
- Features a fullscreen `<canvas>` element (id `#canvas`) for WebGPU rendering.
- Implements a premium glassmorphic UI overlay containing:
  - Interactive buttons and keyboard shortcuts (`1`, `2`, `3`) to switch between rendering geometries (2D Quad, 3D Cube, 3D Sphere).
  - Live performance statistics displaying elapsed time and frame rate (FPS).
  - An error toast overlay to display WebGPU initialization or shader compilation errors.
- Integrates the Emscripten-generated glue script (`engine_wasm.js`).
- Contains the JavaScript render loop (`requestAnimationFrame`) that calculates frame timing, updates uniforms, and drives the engine's tick function.

### [web_context.h](file:///C:/src/shadertoy_webgpu/src/platform/web/web_context.h) & [web_context.cpp](file:///C:/src/shadertoy_webgpu/src/platform/web/web_context.cpp)
Implements the [WebContext](file:///C:/src/shadertoy_webgpu/src/platform/web/web_context.h#L12) class, which inherits from [GraphicsContext](file:///C:/src/shadertoy_webgpu/src/core/graphics_context.h#L7).
- Manages the lifecycle of the WebGPU API in the browser.
- Creates the WebGPU instance and obtains a surface from the HTML canvas selector using `WGPUEmscriptenSurfaceSourceCanvasHTMLSelector`.
- Requests the WebGPU adapter and logical device asynchronously. To handle WebGPU's asynchronous nature within C++ initialization, it polls the future using `emscripten_sleep`.
- Configures the surface and swapchain formats (defaulting to `WGPUTextureFormat_RGBA8Unorm`).
- Implements [GetCurrentSwapChainView](file:///C:/src/shadertoy_webgpu/src/platform/web/web_context.cpp#L184) to retrieve the current texture view for the render pass.
- Implements [Present](file:///C:/src/shadertoy_webgpu/src/platform/web/web_context.cpp#L213) as a no-op, as the web browser automatically presents the canvas surface at the end of the frame.
