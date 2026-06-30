# C++ Engine Source (`src`)

This directory contains the C++ source code for the **Shadertoy WebGPU** application. It is divided into the core engine logic and the platform-specific integration layers.

## Directory Structure

The C++ source is organized into two main subdirectories:

### 1. [core/](file:///C:/src/shadertoy_webgpu/src/core/README.md)
Contains the platform-agnostic core engine. It manages:
- The WebGPU rendering pipeline ([Renderer](file:///C:/src/shadertoy_webgpu/src/core/renderer.h#L43) and its rendering passes).
- The graphics context abstract interface ([GraphicsContext](file:///C:/src/shadertoy_webgpu/src/core/graphics_context.h#L7)).
- Texture and render target management ([TextureManager](file:///C:/src/shadertoy_webgpu/src/core/texture_manager.h#L37)), including feedback-loop ping-ponging.
- Procedural geometry mesh generation ([Mesh](file:///C:/src/shadertoy_webgpu/src/core/mesh.h#L17) for 2D quads, 3D cubes, and 3D spheres).
- Mathematical utilities ([math_utils.h](file:///C:/src/shadertoy_webgpu/src/core/math_utils.h)) and global configurations ([config.h](file:///C:/src/shadertoy_webgpu/src/core/config.h)).
- Shadertoy GLSL compatibility mapping ([shadertoy_compat.h](file:///C:/src/shadertoy_webgpu/src/core/shadertoy_compat.h)).

### 2. [platform/](file:///C:/src/shadertoy_webgpu/src/platform/README.md)
Contains the platform-specific wrappers that implement the abstract `GraphicsContext` interface and manage windowing, input, and shader compilation.
- **[native/](file:///C:/src/shadertoy_webgpu/src/platform/native/README.md)**: Desktop platform integration using Google Dawn (WebGPU), GLFW (windowing and input), and a built-in shader translator (`glslang` + Google `Tint`).
- **[web/](file:///C:/src/shadertoy_webgpu/src/platform/web/README.md)**: Web platform integration compiled via Emscripten. It connects to the browser's WebGPU context and exposes the engine API to JavaScript via Embind.
