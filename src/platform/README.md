# Platform Integration Layer (`src/platform`)

This directory contains the platform-specific integration layers for the **Shadertoy WebGPU** application. It implements the necessary abstractions to run the core rendering engine on both native desktop environments and web browsers.

## Directory Purpose

The `src/platform` directory provides concrete implementations of the abstract [GraphicsContext](file:///C:/src/shadertoy_webgpu/src/core/graphics_context.h#L7) interface. By separating the platform-specific windowing, input, and WebGPU setup from the core renderer, the application can maintain a single, unified rendering codebase (`src/core`).

---

## Subdirectories

### 1. [native/](file:///C:/src/shadertoy_webgpu/src/platform/native/README.md)
Contains the native desktop platform implementation targeting Windows, macOS, and Linux.
- **Windowing**: Uses GLFW to manage window creation and keyboard/mouse events.
- **WebGPU Implementation**: Integrates Google Dawn as the native WebGPU backend.
- **Shader Compilation**: Embeds `glslang` and Google `Tint` to compile GLSL shaders to SPIR-V and translate them to WGSL directly on the host machine.
- **Render Loop**: Implements a high-precision, sub-millisecond desktop render loop (~60 FPS).

### 2. [web/](file:///C:/src/shadertoy_webgpu/src/platform/web/README.md)
Contains the web platform implementation targeting modern web browsers via WebAssembly.
- **WASM compilation**: Configured to compile the C++ codebase to WebAssembly using the Emscripten toolchain.
- **WebGPU Integration**: Interfaces with the browser's native WebGPU API.
- **JS Bindings**: Uses Emscripten's Embind (`bindings.cpp`) to export the engine API (`EngineCore`) to JavaScript, allowing the frontend to control rendering, update uniforms, and load textures.
- **Web UI**: Houses the HTML/JS frontend code providing an interactive, glassmorphic playground interface.
