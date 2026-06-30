# React Frontend Components (`web/src/components`)

This directory contains the React components that form the user interface of the **Shadertoy WebGPU Playground** web application. These components interface with the compiled WebAssembly (WASM) C++ engine to provide a code editor, a rendering viewport, and a compilation console.

## Directory Purpose

The `web/src/components` module is responsible for the user-facing frontend. It orchestrates the WebAssembly lifecycle, captures user code edits and compiles them, renders the WebGPU graphics output, tracks user inputs (mouse and keyboard), and displays compilation diagnostics.

Key features implemented in this module include:
- **Interactive Shader Editing**: CodeMirror-based editor supporting GLSL/WGSL syntax and multi-pass buffer tabs.
- **WebGPU Viewport**: Integration with the WASM engine to render onto an HTML5 canvas, supporting interactive 3D geometries and Shadertoy-style mouse input.
- **Diagnostic Logging**: A terminal-style console displaying compilation info, warnings, and errors from the shader compiler.

---

## File Overview

### [Diagnostics.tsx](file:///C:/src/shadertoy_webgpu/web/src/components/Diagnostics.tsx)
Implements the [Diagnostics](file:///C:/src/shadertoy_webgpu/web/src/components/Diagnostics.tsx#L17) component, which serves as the console and logging panel.
- Displays logs, warnings, and errors emitted by the C++ shader compiler and WebGPU device.
- Features:
  - **Auto-Scroll**: Automatically scrolls to the bottom of the log when new messages are added.
  - **Show WGSL**: A checkbox to toggle the visibility of the translated WGSL source code (when compiling GLSL).
  - **Log Actions**: Buttons to copy all logs to the clipboard (with a fallback for non-secure HTTP contexts) and clear the console.
  - **Visual Coding**: Color-coded log entries based on type (`error` in red, `warning` in yellow, `info` in blue/grey).

### [Editor.tsx](file:///C:/src/shadertoy_webgpu/web/src/components/Editor.tsx)
Implements the [Editor](file:///C:/src/shadertoy_webgpu/web/src/components/Editor.tsx#L21) component, which wraps `@uiw/react-codemirror` (CodeMirror 6).
- Renders the code editor panel where users write their shaders.
- Features:
  - **Pass Tabs**: A tabbed interface to switch editing between different render passes (`BufferA`, `BufferB`, `BufferC`, `BufferD`, and `Image`). Checkboxes allow users to enable or disable individual intermediate buffer passes.
  - **Language Selector**: A toggle to switch between GLSL and WGSL editing modes (applying C++ and Rust syntax highlighters respectively).
  - **Compile Trigger**: A "Compile" button with a loading spinner. Supports the standard compilation keyboard shortcuts (`Alt+Enter` or `Ctrl+Enter`).
  - **Theme**: Custom dark glassmorphic styling matching the application's aesthetic.

### [Viewport.tsx](file:///C:/src/shadertoy_webgpu/web/src/components/Viewport.tsx)
Implements the [Viewport](file:///C:/src/shadertoy_webgpu/web/src/components/Viewport.tsx#L9) component, which hosts the WebGPU canvas and drives the WASM engine.
- Dynamically loads and instantiates the Emscripten WASM module using the window global `createEngineModule`.
- Redirects WASM stdout/stderr logs to the global `onWasmLog` handler so they can be captured by the [Diagnostics](file:///C:/src/shadertoy_webgpu/web/src/components/Diagnostics.tsx#L17) panel.
- Runs the primary JavaScript render loop (`requestAnimationFrame`), calculating frame delta, elapsed time, and FPS.
- Passes frame statistics and mouse coordinates to the WebAssembly engine via `engine.set_uniforms()`.
- **Mouse Input Mapping**: Tracks mouse movements and click states on the canvas. Encodes them to match Shadertoy's `iMouse` conventions (passing click coordinates and using negative values on mouse up).
- **Overlays**:
  - A glassmorphic stats overlay showing live FPS, elapsed time, rendering resolution, and active geometry.
  - A fullscreen error overlay that displays when WebGPU initialization fails (e.g. if the browser or hardware lacks WebGPU support).
- **Cleanup**: Cancels the animation frame and calls `.delete()` on the C++ WebAssembly instance when unmounted to prevent memory leaks.
