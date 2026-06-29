import glslangInit from '@webgpu/glslang/dist/web-devel/glslang.js';
import { lastGlslangErrors, captureState, restoreConsole } from './consoleCapture';

interface Glslang {
  compileGLSL(source: string, stage: string, genDebug: boolean): Uint32Array;
}

interface Twgsl {
  convertSpirV2WGSL(spirv: Uint32Array): string;
}

let glslang: Glslang | null = null;
let twgsl: Twgsl | null = null;
let initPromise: Promise<void> | null = null;

/**
 * Initializes the GLSL-to-WGSL compiler toolchain.
 * Loads both glslang and twgsl WebAssembly modules.
 */
export function initCompiler(glslangWasmPath?: string, twgslWasmPath?: string): Promise<void> {
  if (initPromise) {
    return initPromise;
  }

  initPromise = (async () => {
    const glslangOptions = {
      locateFile: glslangWasmPath ? () => glslangWasmPath : undefined
    };

    // 1. Initialize glslang (always available)
    // NOTE: @webgpu/glslang's glslangInit wrapper ignores custom print/printErr options
    // and binds directly to the global console object during initialization.
    // We temporarily monkey-patch the console before this call (in consoleCapture.ts)
    // and restore it immediately after.
    const glslangModule = await (glslangInit as any)(glslangOptions);
    console.log("glslangModule resolved to:", glslangModule);
    glslang = glslangModule as Glslang;
    console.log("glslang is now:", glslang);
    restoreConsole();

    // 2. Initialize twgsl (optional, from window.twgsl if loaded via script)
    const globalTwgslInit = (window as any).twgsl;
    if (globalTwgslInit) {
      try {
        const twgslModule = await globalTwgslInit(twgslWasmPath);
        twgsl = twgslModule as Twgsl;
        console.log("twgsl initialized successfully.");
      } catch (err) {
        console.warn("Failed to initialize twgsl:", err);
      }
    } else {
      console.warn("twgsl is not available. GLSL-to-WGSL translation will be disabled.");
    }
  })();

  return initPromise;
}

/**
 * Compiles a GLSL shader string into a WGSL shader string.
 * @param glslSource The GLSL shader source code.
 * @param stage The shader stage ('vertex' | 'fragment' | 'compute').
 * @returns The translated WGSL shader source code.
 */
export function compileGLSLToWGSL(glslSource: string, stage: 'vertex' | 'fragment' | 'compute'): string {
  if (!glslang || !twgsl) {
    throw new Error('Compiler not initialized. Call await initCompiler() first.');
  }

  // Clear previous errors before compiling
  lastGlslangErrors.length = 0;
  captureState.isCollecting = true;

  try {
    // 1. Compile GLSL to SPIR-V binary (Uint32Array)
    let spirv: Uint32Array;
    try {
      spirv = glslang.compileGLSL(glslSource, stage, false);
    } catch (err) {
      const errorMsg = lastGlslangErrors.join('\n') || 'GLSL compilation to SPIR-V failed.';
      throw new Error(errorMsg);
    }

    if (!spirv || spirv.length === 0) {
      const errorMsg = lastGlslangErrors.join('\n') || 'GLSL compilation to SPIR-V failed.';
      throw new Error(errorMsg);
    }

    // 2. Translate SPIR-V binary to WGSL string
    const wgsl = twgsl.convertSpirV2WGSL(spirv);
    if (!wgsl) {
      throw new Error('SPIR-V to WGSL translation failed.');
    }

    return wgsl;
  } finally {
    captureState.isCollecting = false;
  }
}
