# Project Plan & Development Roadmap - Shadertoy WebGPU

This document outlines the phased development plan, status tracking, and milestones for the **Shadertoy WebGPU** project. For the technical architecture, specifications, and file structures, please refer to the [Technical Design Document](file:///C:/src/shadertoy_webgpu/docs/DesignDoc.md).

---

## Detailed Implementation Phases

### Phase 1: Platform Abstraction Layer (PAL) & C++ Core Setup
*   Setup `CMakeLists.txt` for dual target compilation (Native Dawn + Emscripten WASM).
*   Create platform abstraction headers (`src/core/graphics_context.h`) and backend bindings (`src/platform/web/` and `src/platform/native/` - with optional `TimestampQuery` checks).
*   Implement core WebGPU renderer: device creation, uniform structures (with padding and `static_assert` statements), MVP matrices.
*   Implement geometry pipeline for fullscreen quads (the foundation for Shadertoy rendering).

### Phase 2: GLSL Compiler Toolchain & Shadertoy Compatibility
*   Web: Integrate `@webgpu/glslang` and `twgsl` NPM packages into the React frontend compiler manager.
*   Native C++: Link Dawn's internal glslang + Tint compiler pipelines.
*   Build the Shadertoy wrapper header `src/core/shadertoy_compat.h` (emulating `mainImage`, `fragCoord`, `iResolution`, `iTime`, `iMouse`, etc.).
*   Wire the compiler pipeline so that GLSL entered by the user is wrapped, compiled to WGSL, and loaded into the C++ core renderer.

### Phase 3: Web UI (TypeScript / React) & Code Editor (Shadertoy Replica)
*   **Phase 3.1: Core WGSL Dashboard**
    *   Scaffold Vite + React (TypeScript) frontend.
    *   Integrate modularized C++ WebAssembly engine with lifecycle memory cleanup and High-DPI (`devicePixelRatio`) rendering.
    *   Implement CodeMirror 6 editor with syntax highlighting for WGSL (via Rust extension fallback).
    *   Build side-by-side "Source Editor" and "Canvas Viewport" panels.
    *   **Diagnostic Interface**: Implement log overlay panels, error lint markers, compile play button with true asynchronous compilation feedback (JS-to-C++ callbacks), and `Alt+Enter` key listener to dynamically update the running shader.
    *   Support mouse coordinate capture and upload (`iMouse` uniform binding).
*   **Phase 3.2: Browser-Side GLSL Compilation**
    *   Integrate `@webgpu/glslang` and `twgsl` (from public registry) into the React frontend compiler manager.
    *   Update `copy-wasm.js` to automatically copy `twgsl.js`/`twgsl.wasm` to the public folder.
    *   Implement GLSL syntax highlighting in CodeMirror (via C++ extension).
    *   Compile GLSL entered by the user to SPIR-V via `glslang`, translate SPIR-V to WGSL via `twgsl`, and upload the translated WGSL code to the C++ core.

### Phase 4: Textures, Multi-Pass & Cubemaps
*   **Phase 4.1: Texture Manager & Multi-Pass (Buffer A..D)**
    *   Implement the multi-pass pipeline runner supporting offscreen render targets.
    *   Implement ping-pong double-buffering logic for frame-feedback shaders checking `"ping_pong"` parameter (enabling Buffer A, B, C, D feedback loops).
    *   Manage sRGB vs Linear (`RGBA8UnormSrgb` vs `RGBA8Unorm`) textures using bindings `"format"` configurations.
    *   Support loading static 2D textures (noise, keyboard, audio, or custom PNGs) and binding them to `iChannel0..3`.
*   **Phase 4.2: Cubemap Support**
    *   **Texture Manager Extensions**: Update `TextureManager` to support allocating 6-layered textures (`depthOrArrayLayers = 6`) and creating views with `WGPUTextureViewDimension_Cube`.
    *   **Cubemap Data Upload**: Support uploading 6 face textures (Positive X, Negative X, Positive Y, Negative Y, Positive Z, Negative Z) to the GPU using layer offsets in `wgpuQueueWriteTexture`.
    *   **Dynamic Shader Declarations**: Refactor the GLSL compatibility header so that `iChannel0..3` are dynamically declared as `textureCube` (for cubemaps) or `texture2D` (for 2D textures) based on the active input configuration, preventing WebGPU texture view binding dimension mismatches.
    *   **Frontend UI Integration**: Update the React input selector to allow selecting cubemaps, uploading cubemap faces, and binding them to channels. Add default cubemap presets (e.g. Skybox, Desert, Forrest).

### Phase 5: Mesh Loader, Scene Manifest & 3D Geometry (Extensions)
*   **Scene Manifest Parser (scene.json)**: Implement a JSON parser in the C++ core (e.g., using a lightweight header-only library) to parse `scene.json` files, dynamically configuring the render pass pipeline, active channel inputs, and texture settings.
*   **ZIP Archive Package Loader**: Integrate **`miniz`** in the C++ core and write wrapper interfaces to read and write `.swgscene` ZIP packages directly, extracting the internal `scene.json` and associated assets in memory.
*   **Native Mesh Loading**: Integrate `tinygltf` and `tinyobjloader` to parse 3D asset file paths specified in the scene manifest.
*   **Web Mesh Loading**: Setup WASM Embind class interfaces (`load_mesh_data`, etc.) passing memory safely via `emscripten::val` copies.
*   Support rendering 3D meshes in early passes and binding their outputs as textures in later post-process passes.
*   Wire IndexedDB asset caching in the browser to store binary meshes and images.

### Phase 6: Native Profiling & Parity Harness
*   Implement native GLFW window wrapper, Dear ImGui overlay menus, and CLI harness to load scenes via **`--scene <path_to_scene_json_or_swgscene>`**, render frames, and take screenshots (`--frame`, `--screenshot`).
*   Implement GPU timestamp queries (`wgpu::FeatureName::TimestampQuery`) for profiling execution duration.
*   **Buffer Dump System**: Add native storage buffer dump capabilities (`--dump-buffer`).
*   **CLI Package Exporter**: Add command-line support to pack/export files (`--save`).

### Phase 7: Compute Shaders & Machine Learning Extensions
*   Implement Compute Pipeline dispatch in C++ core.
*   Support storage buffer bindings and multi-pass compute-to-render chains.
*   Add Web UI storage buffer download utilities.
*   Add ML demo presets.

### Phase 8: GCP Deployment & Secure Hosting (Public Release)
*   Build static production files via `npm run build`.
*   Deploy static files to Firebase Hosting using `firebase deploy`.
