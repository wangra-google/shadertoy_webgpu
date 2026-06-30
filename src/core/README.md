# Core Engine Module (`src/core`)

This directory contains the core engine logic for the **Shadertoy WebGPU** application. It encapsulates the WebGPU rendering pipeline, graphics context abstraction, texture and render target management, procedural geometry generation, and mathematical utilities.

## Directory Purpose

The `src/core` module is responsible for setting up and driving the WebGPU rendering pipeline. It is designed to be platform-agnostic by utilizing a graphics context interface, allowing the same rendering logic to run on both native desktop platforms (via GLFW) and web browsers (via WebAssembly/Emscripten). 

Key features implemented in this module include:
- **Platform Abstraction**: Separation of windowing/swapchain logic from rendering.
- **Multiphase Rendering**: Support for Shadertoy-style multi-pass rendering (Buffer A, B, C, D, and the final Image pass).
- **Shader Compilation**: Real-time compilation of shaders (supporting both WGSL and GLSL-to-WGSL translation) with asynchronous pipeline creation.
- **Feedback Loops**: Ping-pong texture buffering to allow shaders to read from their own output from the previous frame.
- **Interactive 3D**: Standard camera uniform bindings and procedural 3D geometries (cube, sphere) to enable interactive 3D WebGPU scenes.

---

## File Overview

### [config.h](file:///C:/src/shadertoy_webgpu/src/core/config.h)
Defines global configuration constants within the `config` namespace.
- Specifies default window dimensions and title for native desktop applications (`kNativeWindowWidth`, `kNativeWindowHeight`, `kNativeWindowTitle`).
- Specifies default canvas dimensions and selector for Web/WebAssembly targets (`kWebDefaultWidth`, `kWebDefaultHeight`, `kWebCanvasSelector`).

### [graphics_context.h](file:///C:/src/shadertoy_webgpu/src/core/graphics_context.h)
Defines the [GraphicsContext](file:///C:/src/shadertoy_webgpu/src/core/graphics_context.h#L7) abstract interface.
- Decouples the core renderer from platform-specific windowing and swapchain management.
- Provides virtual methods for initializing the WebGPU device, creating the swapchain, fetching the device/queue/surface, retrieving swapchain views, and presenting the frame.

### [math_utils.h](file:///C:/src/shadertoy_webgpu/src/core/math_utils.h)
Provides lightweight 3D math structures and utilities.
- **Structures**:
  - [Vec3](file:///C:/src/shadertoy_webgpu/src/core/math_utils.h#L6): A 3-component floating-point vector.
  - [Mat4](file:///C:/src/shadertoy_webgpu/src/core/math_utils.h#L12): A column-major $4\times4$ matrix.
- **Functions**:
  - Vector operations (addition, subtraction, scaling, length, normalization, dot product, cross product).
  - Matrix operations (identity, multiplication, perspective projection, look-at camera matrix, X/Y rotations, transposition, and matrix inversion).

### [mesh.h](file:///C:/src/shadertoy_webgpu/src/core/mesh.h) & [mesh.cpp](file:///C:/src/shadertoy_webgpu/src/core/mesh.cpp)
Defines the [Vertex](file:///C:/src/shadertoy_webgpu/src/core/mesh.h#L10) structure (containing position, normal, UV, and tangent) and the [Mesh](file:///C:/src/shadertoy_webgpu/src/core/mesh.h#L17) class.
- Wraps WebGPU vertex and index buffers.
- Implements a [Draw](file:///C:/src/shadertoy_webgpu/src/core/mesh.cpp#L49) method that binds buffers to the render pass and issues an indexed draw call.
- Provides static factory methods to generate procedural geometry:
  - [CreateQuad](file:///C:/src/shadertoy_webgpu/src/core/mesh.cpp#L57): Generates a 2D screen-space quad (primarily used for traditional 2D Shadertoy rendering).
  - [CreateCube](file:///C:/src/shadertoy_webgpu/src/core/mesh.cpp#L71): Generates a 3D cube with normals, UVs, and tangents.
  - [CreateSphere](file:///C:/src/shadertoy_webgpu/src/core/mesh.cpp#L126): Generates a 3D UV sphere with configurable subdivisions.

### [renderer.h](file:///C:/src/shadertoy_webgpu/src/core/renderer.h) & [renderer.cpp](file:///C:/src/shadertoy_webgpu/src/core/renderer.cpp)
Implements the main rendering pipeline via the [Renderer](file:///C:/src/shadertoy_webgpu/src/core/renderer.h#L43) class.
- **Uniform Management**:
  - [ShadertoyUniforms](file:///C:/src/shadertoy_webgpu/src/core/renderer.h#L18): Manages standard Shadertoy inputs like `iResolution`, `iTime`, `iTimeDelta`, `iFrameRate`, `iFrame`, `iMouse`, and `iDate`.
  - [CameraUniforms](file:///C:/src/shadertoy_webgpu/src/core/renderer.h#L32): Manages 3D camera matrices (`uViewProj`, `uModel`, `uNormalMatrix`) and camera position.
- **Pass Management**:
  - Coordinates a series of render passes matching Shadertoy's architecture: `BufferA`, `BufferB`, `BufferC`, `BufferD`, and `Image` (the final output).
  - Supports enabling/disabling specific passes and binding textures to input channels (`iChannel0`–`iChannel3`).
- **Shader Compilation**:
  - Handles runtime shader updates (supporting WGSL, and GLSL via shader translation on native).
  - Integrates asynchronous pipeline compilation (`wgpuDeviceCreateRenderPipelineAsync`) to prevent UI/rendering thread freezes during shader recompilation.
- **Viewport Resizing**:
  - Recreates the depth buffer and resizes dynamic render targets to match the window dimensions.

### [shadertoy_compat.h](file:///C:/src/shadertoy_webgpu/src/core/shadertoy_compat.h)
A compatibility layer to facilitate running GLSL Shadertoy shaders within a WebGPU environment.
- Exposes [GLSL_SHADERTOY_HEADER](file:///C:/src/shadertoy_webgpu/src/core/shadertoy_compat.h#L8), a GLSL shader header prepended to user-provided shaders.
- Maps standard Shadertoy inputs to WebGPU bind group layouts (Bind Group 0 for uniforms, Bind Group 1 for textures).
- Defines compatibility macros (e.g., `texture`, `textureLod`, `texelFetch`) that map WebGL-style samplers to separate texture/sampler bindings used by Vulkan/WebGPU, and automatically flips the Y-coordinate to align with WebGPU's texture coordinate space.

### [texture_manager.h](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.h) & [texture_manager.cpp](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.cpp)
Manages texture allocation, lifetime, and binding.
- **Structures**:
  - [TextureResource](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.h#L9): Wraps a `WGPUTexture` and its associated `WGPUTextureView`.
  - [PingPongTexture](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.h#L17): Pairs two `TextureResource` objects to enable read-after-write feedback loops across frames (swapping them via `Swap()`).
- **Key Methods**:
  - [CreateStaticTexture](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.cpp#L125): Uploads CPU pixel data (e.g. sRGB or HDR) to a static GPU texture.
  - [CreateRenderTarget](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.cpp#L165): Allocates textures to serve as render targets for intermediate passes.
  - [ResizeRenderTargets](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.cpp#L211): Resizes all registered dynamic render targets when the viewport size changes.
  - Provides a default 1x1 black texture view and a default bilinear sampler for unbound texture channels.
