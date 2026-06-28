import glslangInit from '@webgpu/glslang';
import twgslInit from 'twgsl';

interface Glslang {
  compileGLSL(source: string, stage: string, genDebug: boolean): Uint32Array;
}

interface Twgsl {
  convertSpirV(spirv: Uint32Array): string;
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
    const glslangOptions = glslangWasmPath ? { locateFile: () => glslangWasmPath } : undefined;
    const twgslOptions = twgslWasmPath ? { locateFile: () => twgslWasmPath } : undefined;

    const [glslangModule, twgslModule] = await Promise.all([
      (glslangInit as any)(glslangOptions),
      (twgslInit as any)(twgslOptions),
    ]);

    glslang = glslangModule as Glslang;
    twgsl = twgslModule as Twgsl;
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

  // 1. Compile GLSL to SPIR-V binary (Uint32Array)
  const spirv = glslang.compileGLSL(glslSource, stage, false);
  if (!spirv || spirv.length === 0) {
    throw new Error('GLSL compilation to SPIR-V failed.');
  }

  // 2. Translate SPIR-V binary to WGSL string
  const wgsl = twgsl.convertSpirV(spirv);
  if (!wgsl) {
    throw new Error('SPIR-V to WGSL translation failed.');
  }

  return wgsl;
}
