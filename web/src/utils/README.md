# Web Utilities Module (`web/src/utils`)

This directory contains utility modules that support the frontend application. Specifically, it implements the browser-side shader compilation toolchain, enabling real-time translation of GLSL shaders into WebGPU-compatible WGSL.

## Directory Purpose

The primary purpose of the `web/src/utils` module is to provide a client-side GLSL-to-WGSL compilation pipeline. Since WebGPU only accepts WGSL natively, this toolchain allows the playground to support traditional GLSL Shadertoy shaders directly in the browser by:
1. Compiling GLSL code to SPIR-V bytecode using `@webgpu/glslang`.
2. Translating SPIR-V bytecode to WGSL source code using `twgsl`.
3. Intercepting and capturing compilation errors printed to the browser console by these WebAssembly libraries.

---

## File Overview

### [compiler.ts](file:///C:/src/shadertoy_webgpu/web/src/utils/compiler.ts)
Manages the initialization and execution of the browser-side shader compilation toolchain.
- **Functions**:
  - [initCompiler](file:///C:/src/shadertoy_webgpu/web/src/utils/compiler.ts#L20): Asynchronously loads the WebAssembly modules for `@webgpu/glslang` (always available) and `twgsl` (loaded from the window object). It temporarily redirects console outputs during `glslang` initialization to prevent console pollution.
  - [compileGLSLToWGSL](file:///C:/src/shadertoy_webgpu/web/src/utils/compiler.ts#L65): Translates GLSL source code to WGSL. 
    - It triggers console capturing, invokes `glslang.compileGLSL` to compile GLSL into SPIR-V, and catches any errors.
    - If successful, it passes the resulting SPIR-V binary (`Uint32Array`) to `twgsl.convertSpirV2WGSL` to generate the final WGSL shader string.

### [consoleCapture.ts](file:///C:/src/shadertoy_webgpu/web/src/utils/consoleCapture.ts)
A utility to intercept and capture messages printed to the global `console` object.
- **Why it is needed**: The `@webgpu/glslang` WebAssembly binary does not return compilation error messages programmatically; instead, it logs them directly to the browser's console.
- **How it works**:
  - Monkey-patches `console.log`, `console.warn`, and `console.error` to intercept arguments and append them to a shared `lastGlslangErrors` array whenever `captureState.isCollecting` is enabled.
  - Passes the intercepted logs through to their original console destinations to maintain normal browser debugging.
  - Provides [restoreConsole](file:///C:/src/shadertoy_webgpu/web/src/utils/consoleCapture.ts#L33) to clean up the monkey-patches and restore the original console functions.
