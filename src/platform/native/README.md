# Native Desktop Platform Module (`src/platform/native`)

This directory contains the native desktop platform implementation for the **Shadertoy WebGPU** application. It manages the application lifecycle on desktop operating systems (Windows, macOS, Linux), handles windowing and input events via GLFW, and provides an offline GLSL-to-WGSL shader compiler.

## Directory Purpose

The `src/platform/native` module serves as the bridge between the C++ engine core (`src/core`) and the host operating system. It implements the desktop-specific graphics context using Dawn (Google's WebGPU implementation) and GLFW, and provides a pipeline to translate traditional GLSL Shadertoy shaders into WebGPU-compliant WGSL shaders.

Key components in this module include:
- **GLFW Windowing & Input**: Creates the application window and processes keyboard and mouse input events.
- **Dawn WebGPU Context**: Native WebGPU integration, including OS-specific surface creation (HWND on Windows).
- **Offline GLSL Compiler**: A compilation pipeline that parses GLSL, compiles it to SPIR-V via `glslang`, and translates it to WGSL via Google `Tint`.
- **High-Precision Loop**: A desktop render loop with sub-millisecond frame pacing to achieve stable frame rates.

---

## File Overview

### [main.cpp](file:///C:/src/shadertoy_webgpu/src/platform/native/main.cpp)
The main entry point for the native desktop application.
- Initializes the global shader compilation libraries (`glslang::InitializeProcess` and `tint::Initialize`).
- Instantiates the [NativeContext](file:///C:/src/shadertoy_webgpu/src/platform/native/native_context.h#L11) and [Renderer](file:///C:/src/shadertoy_webgpu/src/core/renderer.h#L43).
- Supports loading a custom shader file passed as a command-line argument (automatically detecting `.wgsl` vs `.glsl` extensions).
- Registers GLFW callbacks for window resizing ([OnFramebufferResize](file:///C:/src/shadertoy_webgpu/src/platform/native/main.cpp#L56)) and keyboard input ([OnKey](file:///C:/src/shadertoy_webgpu/src/platform/native/main.cpp#L64)) to switch active geometries (Quad, Cube, Sphere).
- Runs the primary render loop:
  - Tracks frame rate (FPS) and elapsed time.
  - Queries mouse position and click states, feeding them into the renderer's uniforms.
  - Invokes the renderer to draw the frame.
  - Implements a precise frame rate pacing mechanism (~60 FPS) that combines sleeping (to reduce CPU usage) with a spin-wait yield loop for sub-millisecond accuracy.
- Coordinates resource shutdown on exit.

### [native_context.h](file:///C:/src/shadertoy_webgpu/src/platform/native/native_context.h) & [native_context.cpp](file:///C:/src/shadertoy_webgpu/src/platform/native/native_context.cpp)
Implements the [NativeContext](file:///C:/src/shadertoy_webgpu/src/platform/native/native_context.h#L11) class, which inherits from [GraphicsContext](file:///C:/src/shadertoy_webgpu/src/core/graphics_context.h#L7).
- Configures GLFW and creates the native OS window.
- Manages the WebGPU instance, adapter, and logical device creation using Dawn.
- Performs OS-specific surface creation. It exposes native Windows integration (`WGPUSurfaceSourceWindowsHWND`) and includes stubs for macOS (`CAMetalLayer`) and Linux (`XlibWindow`).
- Queries the adapter for optional features (e.g., `TimestampQuery`).
- Configures the swapchain texture format (defaulting to `WGPUTextureFormat_BGRA8Unorm`).
- Implements [GetCurrentSwapChainView](file:///C:/src/shadertoy_webgpu/src/platform/native/native_context.cpp#L281) to fetch the next texture view to render into.
- Implements [Present](file:///C:/src/shadertoy_webgpu/src/platform/native/native_context.cpp#L310) which invokes `wgpuSurfacePresent` to display the rendered frame on the screen.

### [shader_compiler.h](file:///C:/src/shadertoy_webgpu/src/platform/native/shader_compiler.h) & [shader_compiler.cpp](file:///C:/src/shadertoy_webgpu/src/platform/native/shader_compiler.cpp)
Implements the offline GLSL shader compilation and translation pipeline.
- [SanitizeGLSL](file:///C:/src/shadertoy_webgpu/src/platform/native/shader_compiler.cpp#L16): Pre-processes GLSL code to rename variables that conflict with reserved WGSL keywords (such as `ref`, `ptr`, `let`, `var`, and `fn`).
- [CompileGLSLToWGSL](file:///C:/src/shadertoy_webgpu/src/platform/native/shader_compiler.cpp#L26):
  - Prepends the Shadertoy compatibility header to GLSL shaders.
  - Uses the `glslang` library to parse the GLSL code and compile it into an intermediate SPIR-V binary.
  - Translates the SPIR-V binary into a WGSL shader string using Google's `Tint` library (`tint::SpirvToWgsl`).
  - Returns the compiled WGSL string or prints diagnostic logs on compilation failure.
