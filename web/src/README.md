# Frontend Source Module (`web/src`)

This directory contains the React TypeScript source code for the **Shadertoy WebGPU Studio** web application. It orchestrates the user interface, manages application state, integrates the WebAssembly C++ rendering engine, and drives the client-side shader compilation toolchain.

## Directory Structure

The module is organized into the following subdirectories and files:

- **[components/](file:///C:/src/shadertoy_webgpu/web/src/components/README.md)**: Contains the React UI components.
  - [Viewport.tsx](file:///C:/src/shadertoy_webgpu/web/src/components/Viewport.tsx): Hosts the WebGPU canvas and drives the WASM engine loop.
  - [Editor.tsx](file:///C:/src/shadertoy_webgpu/web/src/components/Editor.tsx): Renders the multi-pass CodeMirror 6 shader editor.
  - [Diagnostics.tsx](file:///C:/src/shadertoy_webgpu/web/src/components/Diagnostics.tsx): Displays compilation logs, warnings, and errors.
- **[utils/](file:///C:/src/shadertoy_webgpu/web/src/utils/README.md)**: Contains utility modules.
  - [compiler.ts](file:///C:/src/shadertoy_webgpu/web/src/utils/compiler.ts): Interfaces with `glslang` and `twgsl` to translate GLSL to WGSL in the browser.
  - [consoleCapture.ts](file:///C:/src/shadertoy_webgpu/web/src/utils/consoleCapture.ts): Intercepts and captures compilation errors outputted to the console.
- **[App.tsx](file:///C:/src/shadertoy_webgpu/web/src/App.tsx)**: The central React component managing state, file I/O, and toolchain orchestration.
- **[index.css](file:///C:/src/shadertoy_webgpu/web/src/index.css)**: Global stylesheet defining the application's design system and styling tokens.
- **[main.tsx](file:///C:/src/shadertoy_webgpu/web/src/main.tsx)**: The React application entry point.

---

## File Overview (Root Level)

### [App.tsx](file:///C:/src/shadertoy_webgpu/web/src/App.tsx)
The core coordinator of the frontend application. It manages the global state and coordinates interactions between the user, the editor, the WebAssembly compiler, and the rendering engine.
- **State Management**:
  - Tracks the active render pass (`BufferA`–`BufferD` or `Image`).
  - Stores shader source code and languages (GLSL vs WGSL) per pass.
  - Manages which buffer passes are enabled and their corresponding input channel textures (`iChannel0`–`iChannel3`).
  - Manages the active 3D geometry type (2D Quad, 3D Cube, 3D Sphere).
  - Collects compilation logs and errors to display in the console.
- **File I/O**:
  - **Drag and Drop**: Implements a fullscreen drag-and-drop zone. Dropping a `.glsl`, `.wgsl`, or `.txt` file automatically loads it into the active pass and triggers compilation.
  - **Save File**: Exports the current shader source code as a downloadable file named after the active pass (e.g., `BufferA.wgsl`).
- **Engine Orchestration**:
  - Receives the C++ WebAssembly engine instance once the WebGPU context is ready.
  - Generates procedural noise and checkerboard textures and uploads them to the GPU.
  - Intercepts C++ engine logs via `(window as any).onWasmLog` and routes them to the diagnostics panel.
  - Runs GLSL shaders through the [compiler](file:///C:/src/shadertoy_webgpu/web/src/utils/compiler.ts) to translate them to WGSL before sending them to the WebAssembly engine.

### [index.css](file:///C:/src/shadertoy_webgpu/web/src/index.css)
Defines the global styles and design tokens for the application.
- Establishes a premium, dark-themed aesthetic using CSS variables:
  - **Color Palette**: Curated dark background colors (`--bg-main`, `--bg-sidebar`), glassmorphic panels (`--bg-panel`), and neon gradients (`--accent-gradient`).
  - **Typography**: Imports and configures the 'Outfit' font for UI elements and 'Fira Code' for monospaced code elements.
- Implements custom scrollbar styles and animations (e.g., slide-in transitions, pulsing glows).

### [main.tsx](file:///C:/src/shadertoy_webgpu/web/src/main.tsx)
The bootstrap file for the React application.
- Crucially imports [consoleCapture](file:///C:/src/shadertoy_webgpu/web/src/utils/consoleCapture.ts) first, ensuring that console monkey-patching is set up before any WebAssembly modules are loaded.
- Renders the [App](file:///C:/src/shadertoy_webgpu/web/src/App.tsx) component within a `<React.StrictMode>` wrapper and mounts it to the DOM `#root` element.
